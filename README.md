# Task5
from MIPT's 3 semester

It's task about daemon program for backuping directory. Conection with program based on fifo chanel that created in local directory.

Start program:<br>
$ ./run (*path_dir_for_saving) (*path_dir_for_backup) (-auto)

Argument "-auto" assigns the background mode to the program (the program creates a daemons). Without this argument one-time backup is made.

Commands implemented:<br>
^ <b>bcp_dir *path</b> - change backup directory, old backup directory doesn't change and saved<br>
^ <b>cpy_dir *path</b> - change directory for backup<br>
^ <b>info</b> - write current copy and backup directories<br>
^ <b>exit</b> - close daemon<br>
