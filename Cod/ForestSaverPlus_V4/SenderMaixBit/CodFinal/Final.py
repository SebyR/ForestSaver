import sensor, image, time, math, uio
import KPU as kpu
import gc, sys
from machine import UART
from fpioa_manager import fm
from Maix import I2S, FFT, GPIO

# Configuration parameters
SAMPLE_RATE = 11025
SAMPLE_POINTS = 1024
FFT_POINTS = 512
HIST_X_NUM = 128
NUM_CAPTURES_TO_SAVE = 127

INPUT_SIZE = (224, 224)
FFT_INPUT_SIZE = (128, 128)

# Labels for models
FFT_LABELS = ['druj', 'back']
MAIN_LABELS = ['fire', 'none']

cont_druj = 0
cont_foc = 0

# I2S configuration for audio input
fm.register(20, fm.fpioa.I2S0_IN_D0)
fm.register(19, fm.fpioa.I2S0_WS)
fm.register(18, fm.fpioa.I2S0_SCLK)
rx = I2S(I2S.DEVICE_0)
rx.channel_config(rx.CHANNEL_0, rx.RECEIVER, align_mode=I2S.STANDARD_MODE)
rx.set_sample_rate(SAMPLE_RATE)


fm.register(23, fm.fpioa.GPIOHS23)  # Pin ok
gpio_ok = GPIO(GPIO.GPIOHS23, GPIO.IN,  GPIO.PULL_UP)

fm.register(24, fm.fpioa.GPIOHS24)  # Pin FOC
gpio_foc = GPIO(GPIO.GPIOHS24, GPIO.OUT)

fm.register(25, fm.fpioa.GPIOHS25)  # Pin DRUJBA
gpio_drujba = GPIO(GPIO.GPIOHS25, GPIO.OUT)

io_led_green = 12
fm.register(io_led_green, fm.fpioa.GPIO0)
led_g=GPIO(GPIO.GPIO0, GPIO.OUT)

led_g.value(1)

# UART initialization
def init_uart():
    fm.register(10, fm.fpioa.UART1_TX, force=True)
    fm.register(11, fm.fpioa.UART1_RX, force=True)
    return UART(UART.UART1, 115200, 8, 0, 0, timeout=1000, read_buf_len=256)

# Class for handling communication
class Comm:
    def __init__(self, uart):
        self.uart = uart

    def send_classify_result(self, pmax, idx, label):
        print("{}:{:.2f}:{}".format(idx, pmax, label))

# FFT image processing
def fft_img(capture, img, out):
    done = False
    while not done:
        audio = rx.record(SAMPLE_POINTS)
        fft_res = FFT.run(audio.to_bytes(), FFT_POINTS)
        fft_amp = FFT.amplitude(fft_res)
        img_tmp = img.cut(0, 0, 128, 127)
        img.draw_image(img_tmp, 0, 1)
        for i in range(HIST_X_NUM):
            img[i] = fft_amp[i]
        del img_tmp
        imgc = img.to_rainbow(1)
        capture += 1
        if capture % NUM_CAPTURES_TO_SAVE == 0:
            imgc.save("/sd/test.jpeg")
            out = imgc.copy()
            del imgc
            fft_amp.clear()
            done = True
    return capture, img, out

# Main function for FFT and classification
def fft_main(labels, model_addr, comm):
    global cont_druj
    task = None
    try:
        task = kpu.load(model_addr)
        capture = 0
        img = image.Image(size=FFT_INPUT_SIZE).to_grayscale()
        out = image.Image(size=FFT_INPUT_SIZE)
        for _ in range(10):  # Run FFT 10 times
            capture, img, out = fft_img(capture, img, out)
            img_for_model = out.copy()
            img_for_model.pix_to_ai()
            t = time.ticks_ms()
            fmap = kpu.forward(task, img_for_model)
            t = time.ticks_ms() - t
            plist = fmap[:]
            pmax = max(plist)
            max_index = plist.index(pmax)
            comm.send_classify_result(pmax, max_index, labels[max_index].strip())
            if labels[max_index] == 'druj' and pmax > 0.6:
                cont_druj += 1
            time.sleep_ms(10)

        if cont_druj > 3:
            led_g.value(0)
            while gpio_ok.value() == 1:
                print("DRUJBA")
                gpio_drujba.value(1)  # Set DRUJBA GPIO pin HIGH
                time.sleep(0.5)
            led_g.value(1)
            gpio_drujba.value(0)
            cont_druj = 0
    except Exception as e:
        print("Error in fft_main: {e}")
    finally:
        if task:
            kpu.deinit(task)

# Main function for image classification
def main(labels, model_addr, comm, sensor_window=INPUT_SIZE, lcd_rotation=0, sensor_hmirror=False, sensor_vflip=False):
    global cont_foc
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.set_windowing(sensor_window)
    sensor.set_hmirror(sensor_hmirror)
    sensor.set_vflip(sensor_vflip)
    sensor.run(1)

    task = None
    try:
        task = kpu.load(model_addr)
        for _ in range(30):  # Run main classification 30 times
            img = sensor.snapshot()
            img.rotation_corr(z_rotation=270.0, y_rotation= 180)
            t = time.ticks_ms()
            fmap = kpu.forward(task, img)
            t = time.ticks_ms() - t
            plist = fmap[:]
            pmax = max(plist)
            max_index = plist.index(pmax)
            comm.send_classify_result(pmax, max_index, labels[max_index].strip())
            if labels[max_index] == 'fire' and pmax > 0.7:
                cont_foc += 1
            time.sleep_ms(10)
        if cont_foc > 5:
            led_g.value(0)
            while gpio_ok.value() == 1:
                print("FOC")
                gpio_foc.value(1)  # Set FOC GPIO pin HIGH
                time.sleep(0.5)
            led_g.value(1)
            gpio_foc.value(0)
            cont_foc = 0
    except Exception as e:
        print("Error in main: {e}")
    finally:
        if task:
            kpu.deinit(task)

# Entry point
if __name__ == "__main__":
    uart = init_uart()
    comm = Comm(uart)
    while True:
        main(labels=MAIN_LABELS, model_addr="/sd/model-136317.kmodel", comm=comm)
        fft_main(labels=FFT_LABELS, model_addr="/sd/model-136868.kmodel", comm=comm)
