import sensor, image, lcd, time, math
import KPU as kpu
import gc, sys
from machine import UART
from fpioa_manager import fm
from Maix import I2S, FFT, GPIO

sample_rate = 11025
sample_points = 1024
fft_points = 512
hist_x_num = 128

capture = 0
NUM_CAPTURES_TO_SAVE = 127

fm.register(8, fm.fpioa.GPIO0)
wifi_en = GPIO(GPIO.GPIO0, GPIO.OUT)
wifi_en.value(0)
fm.register(20, fm.fpioa.I2S0_IN_D0)
fm.register(19, fm.fpioa.I2S0_WS)    # 30 on dock/bit Board
fm.register(18, fm.fpioa.I2S0_SCLK)  # 32 on dock/bit Board

input_size = (128, 128)
labels = ['druj', 'back']

img = image.Image(size=(128, 128))
img = img.to_grayscale()

out = image.Image(size=(128, 128))

rx = I2S(I2S.DEVICE_0)
rx.channel_config(rx.CHANNEL_0, rx.RECEIVER, align_mode=I2S.STANDARD_MODE)
rx.set_sample_rate(sample_rate)

class Comm:
    def __init__(self, uart):
        self.uart = uart

    def send_classify_result(self, pmax, idx, label):
        # Print the classification result to the terminal
        print("{}:{:.2f}:{}".format(idx, pmax, label))

def fft_img():
    global capture  # Declare that we're using the global variable
    global img
    done = False
    global out
    while not done:
        audio = rx.record(sample_points)
        fft_res = FFT.run(audio.to_bytes(),fft_points)
        fft_amp = FFT.amplitude(fft_res)
        img_tmp = img.cut(0,0,128,127)
        img.draw_image(img_tmp, 0,1)
        for i in range(hist_x_num):
            img[i] = fft_amp[i]
        del(img_tmp)
        imgc = img.to_rainbow(1)

        lcd.display(imgc)
        capture = capture+1


        if capture % NUM_CAPTURES_TO_SAVE == 0:
               imgc.save("/sd/test.jpeg")
               out = imgc.copy()
               del(imgc)  # Clean up after saving
               fft_amp.clear()  # Clear FFT amplitude data after saving
               done = True

def init_uart():
    fm.register(10, fm.fpioa.UART1_TX, force=True)
    fm.register(11, fm.fpioa.UART1_RX, force=True)

    uart = UART(UART.UART1, 115200, 8, 0, 0, timeout=1000, read_buf_len=256)
    return uart

def main(labels=None, model_addr="/sd/m.kmodel", sensor_window=input_size, sensor_hmirror=False, sensor_vflip=False):
    global out
    uart = init_uart()
    comm = Comm(uart)

    try:
        task = kpu.load(model_addr)
        while True:
            fft_img()
            img_for_model = out.copy()
            img_for_model.pix_to_ai()
            t = time.ticks_ms()
            fmap = kpu.forward(task, img_for_model)
            t = time.ticks_ms() - t
            plist = fmap[:]
            pmax = max(plist)
            max_index = plist.index(pmax)
            comm.send_classify_result(pmax, max_index, labels[max_index].strip())
    except Exception as e:
        raise e
    finally:
        if task is not None:
            kpu.deinit(task)

if __name__ == "__main__":
    try:
        main(labels=labels, model_addr="/sd/model-136868.kmodel")
    except Exception as e:
        sys.print_exception(e)
    finally:
        gc.collect()
