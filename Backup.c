#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

//====================================================================================================//

#define MAX_EVENTS 1024 /*Максимальное кличество событий для обработки за один раз*/
#define LEN_NAME 16 /*Будем считать, что длина имени файла не превышает 16 символов*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*размер структуры события*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME )) /*буфер для хранения данных о событиях*/


//====================================================================================================//

int inotify_mode = 0;
int buf_i;
int log_fd = 0;
pid_t pid_parent;
pid_t pid_child;
char path_for_copy_dir[PATH_MAX] = {0};
char path_for_bckp_dir[PATH_MAX] = {0};
time_t rawtime;
struct tm * timeinfo;

void PrintEvent(struct inotify_event *event);
char* Concatenate(char *part1, char *part2);
int GetFullPath(char* path, char *full_path); // Переводит путь к файлу в полный путь
int GetPathToLink(char * path_content, char * full_path, char * delta_path);
int DestInSource(char *destination, char *source); //return 1 if destination in source, 0 if not and -1 if destination doesn't exist, -2 if src
int GetFileSize(int fd);
int CopyFile(char *path_out, char *path_to);
int CopySymLik(char *path_from, char *path_to); // copied link will link to the same content
int ArrEqual(char *arr1, char *arr2);
int RemoveDirectory(char *path);
int RemoveExtra(char *path_from, char *path_to);
int DifferentFiles(char * path_1, char * path_2);
int CopyDir(char *path_out, char *path_to);
int SetInotifyRecursively(char *path, int ino_fd);
void LoopAuto();
char * Find_command(char * txt);
void UpdatingDestWithEvent();

//====================================================================================================//

int main(int argc, char ** argv) {
	if (argc < 3){
		printf("It must be more than 2 arguments\n");
		exit(EXIT_FAILURE);
	}

	strcpy(path_for_copy_dir, argv[1]);
	strcpy(path_for_bckp_dir, argv[2]);

	int dst_in_str = DestInSource(path_for_bckp_dir, path_for_copy_dir);
	if(dst_in_str){
		switch (dst_in_str) {
			case 1:
				printf("destination in source\n");
				exit(EXIT_FAILURE);
			case -1:
				printf("destination does not exist\n");
				break;
			case -2:
				printf("source does not exists\n");
				exit(EXIT_FAILURE);
		}
	}


	char *path_to_log_file = Concatenate(path_for_bckp_dir, "log_backup");
	mkdir(path_for_bckp_dir, 0777);
	log_fd = open(path_to_log_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
	free(path_to_log_file);

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	dprintf(log_fd, "DAEMON %s~ I (file) have been opened by backup program\n", asctime (timeinfo));

	if ((argc == 4) && (!strcmp("-auto", argv[3]))){
		printf("Auto mode activated\n");
		LoopAuto();
		exit(EXIT_FAILURE);
	}

	if (argc == 3){
		RemoveExtra(path_for_copy_dir, path_for_bckp_dir);
		CopyDir(path_for_copy_dir, path_for_bckp_dir);
		close(log_fd);
	}

	return 0;
}

//====================================================================================================//

void LoopAuto(){
	char * command;
	char text[PATH_MAX] = {0};

	int fd_chanel = open("chanel.txt", O_RDWR | O_CREAT, 0666);
	if(fd_chanel == -1){
		perror("DAEMON: open return -1\n");
	}

	int forked = fork();
	switch (forked){
	case 0:
		UpdatingDestWithEvent();
		exit(EXIT_FAILURE);
	case -1:
		printf("Fail: unable to fork\n");
		break;			
	default:
		pid_parent = getpid();
		pid_child = forked;
		break;
	}
	// Code below executing by parent
	//printf("DAEMON: before while(1)\n");

	int ino_chanel = inotify_init();
	inotify_add_watch(ino_chanel, "chanel.txt", IN_CLOSE | IN_OPEN | IN_MODIFY);
	char buf[sizeof(struct inotify_event) + PATH_MAX];

	while(1){
		while (read(ino_chanel, (void *) buf, PATH_MAX) <= 0) {
			;
		}

		//printf("DAEMON: event happens\n");

		memset(text, '\0', PATH_MAX);
		command = text;

		lseek(fd_chanel, SEEK_SET, 0);
		int ret = read(fd_chanel, text, PATH_MAX);

		//printf("DAEMON: read %d\n", ret);
		//memset(command, '\0', PATH_MAX);

		command = Find_command(text);

		printf("DAEMON: GET %s\n", text);
		printf("DAEMON: FIND %s\n", command);


		if (!command){
			continue;
		}

		close(fd_chanel);
		//remove("chanel.txt");
		int fd_chanel = open("chanel.txt", O_RDWR | O_CREAT, 0666);
		if(fd_chanel == -1){
			close(log_fd);
			perror("DAEMON: open return -1\n");
		}

		time_t rawtime;
		struct tm * timeinfo;
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		dprintf(log_fd, "DAEMON %s~ get command:\n%s", asctime (timeinfo), command);

		if(!strncmp(command, "bcp_dir", 7)){
			dprintf(fd_chanel, "DAEMON: get command: bcp_dir\n");
			
			kill(pid_child, SIGKILL);
			close(log_fd);

			char * new_path = strchr(command, ' ');
			new_path++;
			for (int i = 0; i < strlen(new_path); i++){
				if (new_path[i] == '\n'){
					new_path[i] = '\0';
					break;
				}
			}
			
			if (DestInSource(new_path, path_for_copy_dir) == 1){
				printf("DAEMON: user put wrong path for changing backup directory\n");
				dprintf(fd_chanel, "DAEMON: user put wrong path for changing backup directory\n");
				dprintf(log_fd, "~ wrogn path for changing backup directory\n");
				continue;
			}
			
			memset(path_for_bckp_dir, '\0', PATH_MAX);
			strcpy(path_for_bckp_dir, new_path);

			char *path_to_log_file = Concatenate(path_for_bckp_dir, "log_backup");
			mkdir(path_for_bckp_dir, 0777);
			log_fd = open(path_to_log_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
			free(path_to_log_file);

			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			dprintf(log_fd, "DAEMON %s~ I (file) have been opened by backup program\n", asctime (timeinfo));

			forked = fork();
			switch (forked){
			case 0:
				UpdatingDestWithEvent();
				exit(EXIT_FAILURE);
				break;
			case -1:
				printf("Fail: unable to fork\n");
				break;			
			default:
				pid_parent = getpid();
				pid_child = forked;
				break;
			}
			dprintf(fd_chanel, "DAEMON: changes directory for backup to %s\n", path_for_bckp_dir);
		}else if(!strncmp(command, "dest_dir", 7)){
			dprintf(fd_chanel, "DAEMON: get command: dest_dir\n");
			
			kill(pid_child, SIGKILL);

			char * new_path = strchr(command, ' ');
			new_path++;
			for (int i = 0; i < strlen(new_path); i++){
				if (new_path[i] == '\n'){
					new_path[i] = '\0';
					break;
				}
			}
			
			if (DestInSource(path_for_bckp_dir, new_path) == 1){
				printf("DAEMON: user put wrong path for changing backup directory\n");
				dprintf(fd_chanel, "DAEMON: user put wrong path for changing backup directory\n");
				dprintf(log_fd, "~ wrogn path for changing backup directory\n");
				continue;
			}
			
			memset(path_for_copy_dir, '\0', PATH_MAX);
			strcpy(path_for_copy_dir, new_path);

			forked = fork();
			switch (forked){
			case 0:
				UpdatingDestWithEvent();
				exit(EXIT_FAILURE);
				break;
			case -1:
				printf("Fail: unable to fork\n");
				break;			
			default:
				pid_parent = getpid();
				pid_child = forked;
				break;
			}
			dprintf(fd_chanel, "DAEMON: changes source directory to %s\n", path_for_bckp_dir);

		}else if(!strncmp(command, "info", 4)){
			dprintf(fd_chanel, "DAEMON: source directory: %s\n", path_for_copy_dir);
			dprintf(fd_chanel, "DAEMON: backup directory: %s\n", path_for_bckp_dir);
		}else if(!strncmp(command, "exit", 4)){
			kill(pid_child, SIGKILL);
			close(fd_chanel);
			close(log_fd);
			remove("chanel.txt");
			exit(EXIT_SUCCESS);
		}else{
			dprintf(fd_chanel, "DAEMON: get command: unknown command\n");
		}

	}
}

//----------------------------------------------------------------------------------------------------//

void UpdatingDestWithEvent(){
	// log file is already opened
	//log_fd = open(path_to_log_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
	int ino_fd = inotify_init();
	CopyDir(path_for_copy_dir, path_for_bckp_dir);
	SetInotifyRecursively(path_for_copy_dir, ino_fd);
	char buf[sizeof(struct inotify_event) + PATH_MAX];
	while (1){
		while (read(ino_fd, (void *) buf, PATH_MAX) <= 0) {//wait for event
			;
		}
		PrintEvent((struct inotify_event*) buf);
		if(DestInSource(path_for_bckp_dir, path_for_copy_dir) == -1){
			printf("DAEMON: souce has been deleted\n");
			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			dprintf(log_fd, "DAEMON %s~ souce has been deleted\n", asctime (timeinfo));	
			kill(pid_parent, SIGUSR1);
			exit(EXIT_FAILURE);
		}
		RemoveExtra(path_for_copy_dir, path_for_bckp_dir);
		CopyDir(path_for_copy_dir, path_for_bckp_dir);
		close(ino_fd);
		ino_fd = inotify_init();
		SetInotifyRecursively(path_for_copy_dir, ino_fd);
	}
}

//----------------------------------------------------------------------------------------------------//

char * Find_command(char * txt){
	if (txt[strlen(txt) - 1] != '\n'){
		return NULL;
	}
	
	char * dtxt = txt;
	char * ret_pointer;

	int count_D = 0;

	while(1){
		dtxt = strstr(dtxt,"DAEMON");
		if(dtxt){
			count_D++;
			dtxt += 6;
		}else{
			break;
		}
	}

	int count_N = 0;
	dtxt = txt;

	while(1){
		dtxt = strchr(dtxt, '\n');
		if(dtxt){
			count_N++;
			dtxt++;
		}else{
			break;
		}
	}

	printf("FINDER: %d daemon\n", count_D);
	printf("FINDER: %d newline\n", count_N);

	if (count_D == count_N){
		return NULL;
	}else{
		dtxt = txt;
		for (int i = 0; i < count_D; i++){
			dtxt = strchr(dtxt, '\n');
			dtxt++;
		}
		ret_pointer = dtxt;		
	}
	
	return ret_pointer;
}

//----------------------------------------------------------------------------------------------------//

int CopyDir(char *path_out, char *path_to){
	printf("CopyDir, path_out = %s\n", path_out);
	mkdir(path_to, 0777);
	RemoveExtra(path_out, path_to);
	DIR* pdir = opendir(path_out);
	//printf("pdir = %p\n", pdir);
	struct dirent* dt;
	while ((dt = readdir(pdir)) != NULL){
		char *new_path_to = Concatenate(path_to, dt->d_name);
		if(new_path_to == NULL){
			return -1;
		}
		char *new_path_from = Concatenate(path_out, dt->d_name);

		switch (dt->d_type){
			case DT_DIR:
				if(dt->d_name[0] != '.'){
					CopyDir(new_path_from, new_path_to);
				}
				break;
			case DT_REG:
				if (DifferentFiles(new_path_from, new_path_to)) {
					//printf("Files are different\n");
					CopyFile(new_path_from, new_path_to);
				}else{
					//printf("Files are equal\n");
				}
				break;
			case DT_FIFO:
				printf("Get fifo file, skip\n");
				break;
			case DT_LNK:
				printf("Get link, copy link\n");
				CopySymLik(new_path_from, new_path_to);
				break;
			default:
				break;
		}
		free(new_path_from);
		free(new_path_to);
	}
	closedir(pdir);
	return 0;
}

//----------------------------------------------------------------------------------------------------//

void PrintEvent(struct inotify_event *event){
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	printf("In PrintEvent, ");
	if (event->mask & IN_CREATE){
		printf("\"%s\" in create\n", event->name);
		dprintf(log_fd, "DAEMON %s~ \"%s\" in create\n",  asctime(timeinfo), event->name);
	}
	if (event->mask & IN_DELETE){
		printf("\"%s\" in delete\n", event->name);
		dprintf(log_fd, "DAEMON %s~ \"%s\" in delete\n",  asctime(timeinfo), event->name);
	}
	if (event->mask & IN_CLOSE){
		printf("\"%s\" in close\n", event->name);
		dprintf(log_fd, "DAEMON %s~ \"%s\" in close\n",  asctime(timeinfo), event->name);
	}
	if (event->mask & IN_MODIFY){
		printf("\"%s\" in modify\n", event->name);
		dprintf(log_fd, "DAEMON %s~ \"%s\" in modify\n",  asctime(timeinfo), event->name);
	}
	if (event->mask & IN_MOVE){
		printf("\"%s\" in move\n", event->name);
		dprintf(log_fd, "DAEMON %s~ \"%s\" in move\n",  asctime(timeinfo), event->name);
	}
}

//----------------------------------------------------------------------------------------------------//

char* Concatenate(char *part1, char *part2){
	char* result = (char *) calloc(PATH_MAX, sizeof (char));
	buf_i = 0;
	while (part1[buf_i] != 0){
		result[buf_i] = part1[buf_i];
		buf_i++;
		if(buf_i == PATH_MAX - 1){
			free(result);
			return NULL;
		}
	}
	if(result[buf_i - 1] != '/') {
		result[buf_i] = '/';
		buf_i++;
	}
	if(buf_i == PATH_MAX - 2){
		free(result);
		return NULL;
	}
	int len_part_1 = buf_i;
	while (part2[buf_i - len_part_1] != 0){
		result[buf_i] = part2[buf_i - len_part_1];
		buf_i++;
		if(buf_i == PATH_MAX - 1){
			free(result);
			return NULL;
		}
	}
	//result[buf_i] = '/';
	return result;
}

//----------------------------------------------------------------------------------------------------//

int GetPathToLink(char * path_content, char * full_path, char * delta_path){
	//printf("GetPathToLink get arg:\n%s %s %s\n", path_content, full_path, delta_path);

	int aar = 0;

	aar = strrchr(delta_path, '/') - delta_path;
	if (aar == -1){
		printf("In CopySymLink strrchr returned %d\n", aar);
		return -1;
	}

	for (int i = aar + 1; i < PATH_MAX; i++){
		delta_path[i] = '\0';
	}

	//printf("new delta_path %s\n", delta_path);

	if (path_content[0] != '/'){
		path_content = Concatenate(delta_path, path_content);
	}

	aar = GetFullPath(path_content, full_path);

	if (aar == -1){
		printf("In CopySymLink GetFullPath returned %d\n", aar);
		return -1;
	}
	return 1;
}

//----------------------------------------------------------------------------------------------------//

int GetFullPath(char* path, char *full_path){

	printf("getfullpath get arg:\n%s %s\n", path, full_path);
	if(path[0] == '/'){
		int i = 0;
		while (path[i] != 0){
			full_path[i] = path[i];
			i++;
		}
		return 1;
	}

	getcwd(full_path, PATH_MAX);
	char *concatenated = Concatenate(full_path, path);
	printf("concatenated = %s\n", concatenated);
	if(realpath(concatenated, full_path) == NULL){
		free(concatenated);
		perror("realpath return NULL\n");
		return -1;
	}
	//printf("fullpath = %s\n", full_path);
	free(concatenated);
	return 1;
}

//----------------------------------------------------------------------------------------------------//

int DestInSource(char *destination, char *source){
	char *full_path_dst = (char *) calloc(PATH_MAX, sizeof (char));
	char *full_path_src = (char *) calloc(PATH_MAX, sizeof (char));
	if(GetFullPath(destination, full_path_dst) == -1){
		free(full_path_src);
		free(full_path_dst);
		return -1;
	}
	if(GetFullPath(source, full_path_src) == -1){
		free(full_path_src);
		free(full_path_dst);
		return -2;
	}
	//printf("full_path_dst = %s, full_path_src = %s\n", full_path_dst, full_path_src);
	int i = 0;
	while (full_path_src[i] != 0 && full_path_src[i] == full_path_dst[i]){
		i++;
	}
	if(full_path_src[i] == 0 || full_path_src[i+1] == 0){
		printf("i = %d, full_path_src[i] = %d\n", i, full_path_src[i]);
		free(full_path_src);
		free(full_path_dst);
		return 1;
	}
	free(full_path_src);
	free(full_path_dst);
	return 0;
}

//----------------------------------------------------------------------------------------------------//

int GetFileSize(int fd){
	if(fd == -1)
		return -1;

	struct stat buf;
	fstat(fd, &buf);
	off_t file_size = buf.st_size;
	return file_size;
}

//----------------------------------------------------------------------------------------------------//

int CopyFile(char *path_out, char *path_to){
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	dprintf(log_fd, "DAEMON %s~ copying file %s to %s\n", asctime (timeinfo), path_out, path_to);
	//printf("CopyFile%s\n", path_out);
	int   c;
	FILE *stream_R;
	FILE *stream_W;

	stream_R = fopen (path_out, "r");
	if (stream_R == NULL)
		return -1;
	stream_W = fopen (path_to, "w");   //create and write to file
	if (stream_W == NULL)
	{
		fclose (stream_R);
		return -2;
	}
	while ((c = fgetc(stream_R)) != EOF)
		fputc (c, stream_W);
	fclose (stream_R);
	fclose (stream_W);

	return 0;
}

//----------------------------------------------------------------------------------------------------//

int CopySymLik(char *path_from, char *path_to){
	// copied link will link to the same content

	printf("symlink get arg:\n%s %s\n", path_from, path_to);
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	char *link_content = calloc(PATH_MAX, sizeof (char));
	int n_read = readlink(path_from, link_content, PATH_MAX);
	if (n_read == -1){
		free(link_content);
		printf("readlink returned -1\n");
		return -1;
	}

	char* full_link_content = calloc(PATH_MAX, sizeof (char));
	int got_full_path = GetPathToLink(link_content, full_link_content, path_from);
	free(link_content);
	if (got_full_path == -1){
		free(full_link_content);
		printf("In CopySymLink GetPathToLink returned %d\n", got_full_path);
		return -1;
	}

	int symlinked = symlink(full_link_content, path_to);
	free(full_link_content);
	if (symlinked == -1){
		printf("In CopySymLink symlink returned -1\n");
		return -1;
	}
	dprintf(log_fd, "DAEMON %s~ copying symlink %s to %s\n", asctime (timeinfo), path_from, path_to);
	return 0;
}

//----------------------------------------------------------------------------------------------------//

int ArrEqual(char *arr1, char *arr2){
	int i = 0;
	while (arr1[i] != 0 && arr2[i] != 0){
		if(arr1[i] != arr2[i]){
			return 0;
		}
		i++;
	}
	return 1;
}

//----------------------------------------------------------------------------------------------------//

int RemoveDirectory(char *path) {
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	dprintf(log_fd, "DAEMON %s~ removing %s\n", asctime (timeinfo), path);
	printf("RemoveDirectory %s\n", path);
	DIR *d = opendir(path);
	size_t path_len = strlen(path);
	int r = -1;

	if (d) {
		struct dirent *p;

		r = 0;
		while (!r && (p=readdir(d))) {
			int r2 = -1;
			char *buf;
			size_t len;

			/* Skip the names "." and ".." as we don't want to recurse on them. */
			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
				continue;

			len = path_len + strlen(p->d_name) + 2;
			buf = malloc(len);

			if (buf) {
				struct stat statbuf;

				snprintf(buf, len, "%s/%s", path, p->d_name);
				if (!stat(buf, &statbuf)) {
					if (S_ISDIR(statbuf.st_mode))
						r2 = RemoveDirectory(buf);
					else
						r2 = unlink(buf);
				}
				free(buf);
			}
			r = r2;
		}
		closedir(d);
	}

	if (!r)
		r = rmdir(path);

	return r;
}

//----------------------------------------------------------------------------------------------------//

int RemoveExtra(char *path_from, char *path_to){
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	dprintf(log_fd, "DAEMON %s~ removing extra in %s\n", asctime (timeinfo), path_to);
	DIR* pdir_from = opendir(path_from);
	if(pdir_from == 0){
		return -1;
	}
	DIR* pdir_to = opendir(path_to);
	if(pdir_to == 0){
		return -1;
	}

	struct dirent* dt_from;
	struct dirent* dt_to;
	int it_exist; // флаг на существование файла в директории dt_to
	while ((dt_to = readdir(pdir_to)) != NULL){
		if (strcmp(dt_to->d_name, "log_backup")) {
			it_exist = 0;
			while ((dt_from = readdir(pdir_from)) != NULL) {
				if (ArrEqual(dt_to->d_name, dt_from->d_name)) {
					it_exist++;
					break;
				}
			}
			if (!it_exist) { //значит удаляем
				char *new_path = Concatenate(path_to, dt_to->d_name);
				if (dt_to->d_type == DT_DIR) {
					RemoveDirectory(new_path);
				} else {
					printf("Unlinkl %s\n", new_path);
					unlink(new_path);
				}
				free(new_path);
			}
			rewinddir(pdir_from);
		}
	}
	rewinddir(pdir_to);
	closedir(pdir_to);
	closedir(pdir_from);
	return 0;
}

//----------------------------------------------------------------------------------------------------//

int DifferentFiles(char * path_1, char * path_2) {
	int file1 = open(path_1, O_RDONLY);
	int file2 = open(path_2, O_RDONLY);
	if (file2 == -1) {
		//printf("comparator returning cause does not exist ERRNO = %i\n", errno);
		close(file1);
		close(file2);
		return 1;
	}

	struct stat info1;
	fstat(file1, &info1);

	struct stat info2;
	fstat(file2, &info2);

	struct timespec time_from, time_to;

	time_from = info1.st_mtim;
	time_to = info2.st_mtim;

	int time = (time_from.tv_sec - time_to.tv_sec);
	//printf("comparator returning 0\n");
	close(file1);
	close(file2);
	if (time > 0) {
		return 1;
	}
	return 0;
}

//----------------------------------------------------------------------------------------------------//

int SetInotifyRecursively(char *path, int ino_fd){
	inotify_add_watch(ino_fd, path, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE);
	//printf("Set inotify, path = %s, added = %d\n", path, added);
	DIR* pdir = opendir(path);
	struct dirent* dt;
	while ((dt = readdir(pdir)) != NULL){
		char *new_path = Concatenate(path, dt->d_name);
		if (dt->d_type == DT_DIR && dt->d_name[0] != '.'){
			SetInotifyRecursively(new_path, ino_fd);
		}
		free(new_path);
	}
	closedir(pdir);
	return 0;
}
