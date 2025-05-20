# main.py

from fpioa_manager import fm
from machine import UART
import sensor, lcd, time, os, gc

lcd.init()
lcd.direction(lcd.YX_LRUD)
lcd.clear()
lcd.draw_string(10, lcd.height()//2 - 10, "Checking SD...", lcd.WHITE, lcd.RED)
time.sleep(1)

try:
    os.listdir('/sd')
except OSError:
    lcd.clear((255, 0, 0))
    lcd.draw_string(10, lcd.height()//2 - 30, "ERROR:", lcd.WHITE, lcd.RED)
    lcd.draw_string(10, lcd.height()//2 + 10, "No SD card!", lcd.WHITE, lcd.RED)
    while True:
        time.sleep(1)

lcd.clear()
lcd.draw_string(10, lcd.height()//2 - 10, "SD OK", lcd.WHITE, lcd.RED)
time.sleep(1)

fm.register(21, fm.fpioa.UART2_RX)
fm.register(22, fm.fpioa.UART2_TX)
uart2 = UART(UART.UART2, 115200, timeout=1000)

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QQVGA)  # 160×120
sensor.run(1)

lcd.clear()
print("MaixDuino: Ready, SD mounted")

while True:
    img = sensor.snapshot()
    lcd.display(img)

    if uart2.any():
        cmd = ''
        try:
            cmd = uart2.read().decode('utf-8','ignore').strip()
            print("CMD:", cmd)
        except:
            print("encdoe erroe")
        if cmd == "SNAPSHOT":
            w, h = img.width(), img.height()
            uart2.write("RAW:%d,%d\n" % (w, h))
            for y in range(h):
                row = bytearray()
                for x in range(w):
                    # получаем RGB888
                    r, g, b = img.get_pixel(x, y)
                    # конвертируем в RGB565: R5-G6-B5
                    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    # low byte, high byte
                    row.append(rgb565 & 0xFF)
                    row.append((rgb565 >> 8) & 0xFF)
                uart2.write(row)
                time.sleep_ms(5)
            print("Image streamed")


    time.sleep_ms(50)
