# Task5
from MIPT's 3 semester

It's task about demon program for backuping directory. Conection with program based on fifo.

Start program:
$ ./run (*path_dir_for_saving) (*path_dir_for_backup) -key

Argument -key

Commands implemented:
^ dir_bcp *path - change backup directory, old backup directory doesn't change and saved
^ dir_cpy *path - change directory for backup
^ log - write log file (change histrory)
^ auto - switch mode of working: automatically backup file after changes in copied directory (or) backup file only after using command
^ backup - make backup immediately
