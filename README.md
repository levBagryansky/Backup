# Task5
from MIPT's 3 semester

It's task about demon program for backuping directory. Conection with program based on fifo.

Start program:
$ ./run (*path_dir_for_saving) (*path_dir_for_backup) -key

Argument -key

Commands implemented:
^ <b>dir_bcp *path</b> - change backup directory, old backup directory doesn't change and saved<br>
^ <b>dir_cpy *path</b> - change directory for backup<br>
^ <b>log</b> - write log file (change histrory)<br>
^ <b>auto</b> - switch mode of working: automatically backup file after changes in copied directory (or) backup file only after using command<br>
^ <b>backup</b> - make backup immediately<br>
