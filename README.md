# Task5
from MIPT's 3 semester

It's task about daemon program for backuping directory. Conection with program based on fifo chanel that created in local directory.

Start program:<br>
$ ./run (*path_dir_for_saving) (*path_dir_for_backup) (-auto)

Argument "-auto" assigns the background mode to the program (the program creates a daemons). Without this argument one-time backup is made.

Commands implemented:<br>
^ <b>dir_bcp *path</b> - change backup directory, old backup directory doesn't change and saved<br>
^ <b>dir_cpy *path</b> - change directory for backup<br>
^ <b>log</b> - write log file (change histrory)<br>
^ <b>auto</b> - switch mode of working: automatically backup file after changes in copied directory (or) backup file only after using command<br>
^ <b>backup</b> - make backup immediately<br>
^ <b>exit</b> - close daemon<br
^ <b>term *sec</b> - designates the time in seconds between automatic backups<br>
 