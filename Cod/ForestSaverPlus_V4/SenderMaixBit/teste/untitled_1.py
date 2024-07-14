def sd_check():
    import os
    try:
        os.listdir("sd/")
    except Exception as e:
        return False
    return True
print(sd_check())
