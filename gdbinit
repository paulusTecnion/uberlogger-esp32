target remote :3333
adapter_khz 10000
set remote hardware-watchpoint-limit 2
mon reset halt
maintenance flush register-cache
thb app_main
c