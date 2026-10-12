# LM75 I2C Temperature Sensor Linux Driver

Raspberry Pi 3（kernel 4.14.98）上的LM75B I2C client driver包含sysfs溫度介面與O.S.過溫中斷警報

## 功能

- 以`i2c_transfer()`讀取溫度（11 bit，解析度 0.125°C）
- sysfs：`temp`（唯讀）、`overtemp`（可讀寫）
- O.S.腳 → GPIO18 → IRQ → 蜂鳴器（GPIO16）警報
- 透過 Device Tree overlay與driver配對（`compatible = "lm75bd"`）

## 硬體接線

| 訊號 | RPi 腳位 |
|---|---|
| SDA / SCL | GPIO2 / GPIO3（i2c-1） |
| O.S. | GPIO18 |
| 蜂鳴器 | GPIO16 |
| VCC / GND | 3.3V / GND |

模組板A0~A2接地，I2C位址為`0x48`。

## 架構

```
User space   cat / echo  /sys/bus/i2c/devices/1-0048/{temp,overtemp}
                 │
Kernel       sysfs → temp_show / overtemp_show / overtemp_write
                 │
             lm75 driver ── i2c_transfer() ── i2c-core ── i2c-bcm2835 ── LM75
                 ▲
             lm75_isr ◀── IRQ ◀── GPIO18 ◀── O.S. 腳
                 │
             GPIO16 蜂鳴器
```

## 對照 LM75 datasheet

### 1. Client address

位址 = `1001` + `A2 A1 A0`。A2~A0 全接地 → `1001000` = **0x48**（7 bit）。Device tree 的 `reg = <0x48>` 與此一致。

### 2. 溫度暫存器

| 暫存器 | Pointer | R/W | 預設 |
|---|---|---|---|
| Temp | `0x00` | 唯讀 | n/a |
| Conf | `0x01` | 讀寫 | `0x00` |
| Thyst | `0x02` | 讀寫 | `0x4B00`（75°C） |
| Tos | `0x03` | 讀寫 | `0x5000`（80°C） |

Temp：contains two 8-bit data bytes; to store the measured Temp data.

### 3. Temperature register 資料格式

唯讀，2 個 byte（MSByte 先送、LSByte 後送）。只有最高 11 bit 有效，LSByte 低 5 bit 為 0，應忽略：

```
MSByte:  D10 D9 D8 D7 D6 D5 D4 D3
LSByte:  D2  D1 D0 X  X  X  X  X
```

### 4. 溫度換算

1. D10 = 0：正溫度，Temp value (°C) = +(Temp data) × 0.125
2. D10 = 1：負溫度，Temp value (°C) = −(Temp data 的二補數) × 0.125

實作（kernel 不使用浮點，輸出單位為 milli-degree Celsius）：

```c
raw = (buf[0] << 8) | buf[1];      /* signed short，bit15 即符號位元 */
milldeg = (raw >> 5) * 125;        /* 有號右移保留符號；1 LSB = 125 m°C */
```

寫入為反向：`raw = (milldeg / 125) * 32`，再拆成 MSB/LSB。

驗算：`0x1900 >> 5 = 200`，`200 × 125 = 25000` → 25.0°C；預設 `0x5000` → 80000 → 80°C。

> 開發紀錄：初版誤用 9 bit 解讀（`>> 7`、`* 500`，解析度 0.5°C），對照 datasheet 後修正為 11 bit。

## 透過 sysfs 讀取溫度

```c
static DEVICE_ATTR(temp, S_IRUGO, temp_show, NULL);   /* temp_show：被讀時呼叫的函式 */

/* probe 之中交給 kernel */
device_create_file(&client->dev, &dev_attr_temp);
```

流程：`cat temp` → sysfs → `dev_attr_show()` → `temp_show()` → `lm75_read_temp()` → `i2c_transfer()` → 回傳字串。`remove` 時以 `device_remove_file()` 還原。

`.show` / `.store` 的函式簽名由kernel固定：

```c
ssize_t show (struct device *dev, struct device_attribute *attr, char *buf);
ssize_t store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
```

## 中斷（過溫警報）

1. 查datasheet register list，找到過溫暫存器：
   Overtemperature shutdown threshold register: contains two 8-bit data bytes; to store the overtemperature shutdown Tth(ots) limit; default = 80°C。
   確認 **pointer = `0x03`**（Tos）。
2. 讀取預設overtemp，確認為 80°C（`80000 mC`）。
3. 寫入overtemp暫存器（driver預設寫入30°C，Thyst24°C，方便以手捏晶片進行測試

O.S. 腳為開汲極，GPIO18以`gpio_to_irq()`取得IRQ，雙邊緣觸發。
ISR讀取腳位電位：
- LOW：過溫 → 蜂鳴器 ON
- HIGH：警報解除 → 蜂鳴器 OFF

## 編譯與使用

環境：Raspberry Pi 3、kernel 4.14.98

```bash
make
sudo insmod lm75.ko
dmesg | tail

cat /sys/bus/i2c/devices/1-0048/temp                 # 例：22500（milli-degree）
cat /sys/bus/i2c/devices/1-0048/overtemp             # 目前 Tos
echo 28000 > /sys/bus/i2c/devices/1-0048/overtemp    # 設定 Tos = 28°C（milli-degree）

sudo rmmod lm75
```
Demo：以手指捏住晶片使溫度超過 Tos → O.S. 動作 → `dmesg` 出現 `LM75 OS LOW: overtemperature`，蜂鳴器響；降到Thyst以下後解除
