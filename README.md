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
If you place it differently update CMakeList.txt<br>
### esp32_common/cmds
implements system and wifi utility commands
### esp32_common/tcp
implements mqtt client, NTP sync and log redirection over TCP
### esp32_common/utils
implements ota utility, printf wrappers and a spiffs read/write function
