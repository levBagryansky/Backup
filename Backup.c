#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>
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

void PrintEvent(struct inotify_event *event);
char* Concatinate(char *part1, char *part2);
int GetFullPath(char* path, char *full_path); // Переводит путь к файлу в полный путь
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
	char *path_to_log_file = Concatinate(path_for_bckp_dir, "log_backup");
	log_fd = open(path_to_log_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
	free(path_to_log_file);

	if ((argc == 4) && (!strcmp("-auto", argv[3]))){
		mkdir(path_for_bckp_dir, 0777);
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
		dprintf(log_fd, "\ntime %sget command %s\n", asctime (timeinfo), command);

		if(!strncmp(command, "bcp_dir", 7)){
			kill(pid_child, SIGKILL);

			command = strchr(command, ' ');
			command++;

			memset(path_for_bckp_dir, '\0', PATH_MAX);
			strcpy(path_for_bckp_dir, command);

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
		}else if(!strncmp(command, "cpy_dir", 7)){
			kill(pid_child, SIGKILL);
			dprintf(fd_chanel, "DAEMON: get command: cpy_dir\n");
			dprintf(fd_chanel, "DAEMON: I do nothing)\n");

		}else if(!strncmp(command, "info", 4)){
			dprintf(fd_chanel, "DAEMON: copy directory: %s\n", path_for_copy_dir);
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
	char *path_to_log_file = Concatinate(path_for_bckp_dir, "log_backup");
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
		char *new_path_to = Concatinate(path_to, dt->d_name);
		if(new_path_to == NULL){
			return -1;
		}
		char *new_path_from = Concatinate(path_out, dt->d_name);

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
		dprintf(log_fd, "\ntime: %s\"%s\" in create\n",  asctime(timeinfo), event->name);
	}
	if (event->mask & IN_DELETE){
		printf("\"%s\" in delete\n", event->name);
		dprintf(log_fd, "\ntime: %s, \"%s\" in delete\n",  asctime(timeinfo), event->name);
	}
	if (event->mask & IN_CLOSE){
		printf("\"%s\" in close\n", event->name);
		dprintf(log_fd, "\ntime: %s, \"%s\" in close\n",  asctime(timeinfo), event->name);
	}
	if (event->mask & IN_MODIFY){
		printf("\"%s\" in modify\n", event->name);
		dprintf(log_fd, "\ntime: %s, \"%s\" in modify\n",  asctime(timeinfo), event->name);
	}
}

//----------------------------------------------------------------------------------------------------//

char* Concatinate(char *part1, char *part2){
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

int GetFullPath(char* path, char *full_path){
	if(path[0] == '/'){
		int i = 0;
		while (path[i] != 0){
			full_path[i] = path[i];
			i++;
		}
		return 1;
	}

	getcwd(full_path, PATH_MAX);
	char *concatenated = Concatinate(full_path, path);
	//printf("concatenated = %s\n", concatenated);
	if(realpath(concatenated, full_path) == NULL){
		free(concatenated);
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
	dprintf(log_fd, "\ntime %s: copying file %s to %s\n", asctime (timeinfo), path_out, path_to);
	//printf("CopyFile%s\n", path_out);
	int fd_in = open(path_out, O_RDONLY);
	int fd_out = open(path_to, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if(fd_out == -1){
		return -1;
	}
	int size = GetFileSize(fd_in);
	char* src = mmap(0, size, PROT_READ, MAP_SHARED, fd_in, 0);
	if(src == NULL){
		return -1;
	}
	lseek(fd_out, size - 1, SEEK_SET);
	write(fd_out, "", 1);
	char* dst = mmap(0, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_out, 0);
	if(dst == NULL){
		return -1;
	}
	memcpy(dst, src, size);
	close(fd_out);
	close(fd_in);
	return 0;
}

//----------------------------------------------------------------------------------------------------//

int CopySymLik(char *path_from, char *path_to){
	// copied link will link to the same content
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	dprintf(log_fd, "\ntime %s: copying symlink %s to %s\n", asctime (timeinfo), path_from, path_to);

	char *link_content = calloc(PATH_MAX, sizeof (char));
	int n_read = readlink(path_from, link_content, PATH_MAX);
	if (n_read == -1){
		free(link_content);
		printf("readlink returned -1\n");
		return -1;
	}

	char* full_link_content = calloc(PATH_MAX, sizeof (char));
	int got_full_path = GetFullPath(link_content, full_link_content);
	free(link_content);
	if (got_full_path == -1){
		free(full_link_content);
		printf("In CopySymLink GetFullPath returned %d\n", got_full_path);
		return -1;
	}

	int symlinked = symlink(full_link_content, path_to);
	free(full_link_content);
	if (symlinked == -1){
		printf("In CopySymLink symlink returned -1\n");
		return -1;
	}
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
	dprintf(log_fd, "\ntime %s: removing %s\n", asctime (timeinfo), path);
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
	dprintf(log_fd, "\ntime %sremoving extra in %s\n", asctime (timeinfo), path_to);
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
		if (strcmp(dt_to->d_name, ".log_backup")) {
			it_exist = 0;
			while ((dt_from = readdir(pdir_from)) != NULL) {
				if (ArrEqual(dt_to->d_name, dt_from->d_name)) {
					it_exist++;
					break;
				}
			}
			if (!it_exist) { //значит удаляем
				char *new_path = Concatinate(path_to, dt_to->d_name);
				if (dt_to->d_type == DT_DIR) {
					RemoveDirectory(new_path);
				} else {
					printf("Unlinl %s\n", new_path);
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
	inotify_add_watch(ino_fd, path, IN_CREATE | IN_DELETE | IN_MODIFY);
	//printf("Set inotify, path = %s, added = %d\n", path, added);
	DIR* pdir = opendir(path);
	struct dirent* dt;
	while ((dt = readdir(pdir)) != NULL){
		char *new_path = Concatinate(path, dt->d_name);
		if (dt->d_type == DT_DIR && dt->d_name[0] != '.'){
			SetInotifyRecursively(new_path, ino_fd);
		}
		free(new_path);
	}
	closedir(pdir);
	return 0;
}
