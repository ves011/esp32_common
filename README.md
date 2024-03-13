# esp32_common
holds common components for esp32 applications
The current CMakeList.txt places this folder at the same level with application folder<br>
&nbsp;&nbsp;&nbsp;&nbsp;--/application folder<br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;--/main <br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;--/... <br>
&nbsp;&nbsp;&nbsp;&nbsp;--/esp32_common<br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;--/cmds <br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;--/tcp <br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;--/utils <br>
If you place it differently update CMakeList.txt<br><br>
The picture below shows how framework is initialized with respect the common components.
![plot](../esp32wp_controller/doc/init.png)
### esp32_common/cmds
implements system and wifi utility commands
> Note about <b>console</b> command
>> console on | off | tcp
>>> on --> display messages (printfs, ESP_LOGx) on the serial console<br>
>>> tcp --> redirects all messages to LOG_SERVER listening on LOG_PORT (tcp_log.h)
>>>> on the server machine just run <b>ncat -l LOG_PORT -k</b><br>

>>> off --> mute all your messages, but IDF components using printf directly cannot be muted

### esp32_common/comm
implements SPI wrapper required by communication with AD7811
### esp32_common/AD7811
implements primitives to start conversion and read data for AD7811
### esp32_common/tcp
implements mqtt client app site, NTP sync and log redirection over TCP
### esp32_common/utils
implements ota utility, printf wrappers and a spiffs read/write function
