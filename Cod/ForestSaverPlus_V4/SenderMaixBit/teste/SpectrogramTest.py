from Maix import GPIO, I2S, FFT
import image, lcd, math
from fpioa_manager import fm
from time import sleep_ms
import os

# Initialization
sample_rate = 11025
sample_points = 1024
fft_points = 512
hist_x_num = 128
spectrogram_height = 128
capture = 0

# Initialize LCD
lcd.init()
lcd.rotation(2)

# Close WiFi
fm.register(8,  fm.fpioa.GPIO0)
wifi_en = GPIO(GPIO.GPIO0, GPIO.OUT)
wifi_en.value(0)

# Register I2S pins
fm.register(20, fm.fpioa.I2S0_IN_D0)
fm.register(30, fm.fpioa.I2S0_WS)
fm.register(32, fm.fpioa.I2S0_SCLK)

# Initialize I2S
rx = I2S(I2S.DEVICE_0)
rx.channel_config(rx.CHANNEL_0, rx.RECEIVER, align_mode=I2S.STANDARD_MODE)
rx.set_sample_rate(sample_rate)

# Create an empty spectrogram image
spectrogram_img = image.Image(size=(hist_x_num, spectrogram_height), color=255)
spectrogram_img = spectrogram_img.to_grayscale()

while True:
    audio = rx.record(sample_points)
    fft_res = FFT.run(audio.to_bytes(), fft_points)
    fft_amp = FFT.amplitude(fft_res)

    # Shift the image up by one pixel
    img_tmp = spectrogram_img.cut(0, 1, hist_x_num, spectrogram_height - 1)
    spectrogram_img.draw_image(img_tmp, 0, 0)

    # Draw the new FFT amplitude values at the bottom row
    for i in range(hist_x_num):
        color = int(fft_amp[i])
        if color > 255:
            color = 255
        spectrogram_img.set_pixel(i, spectrogram_height - 1, color)

    del img_tmp
    fft_amp.clear()

    # Display the spectrogram on the LCD
    lcd.display(spectrogram_img)

    # Save the spectrogram image to SD card

    savepath = "/sd/spectrogram_" + str(capture) + ".bmp"
    spectrogram_img.save(savepath)
    print("Spectrogram saved to", savepath)
    capture += 1

    sleep_ms(30)
