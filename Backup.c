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


#define MAX_EVENTS 1024 /*Максимальное кличество событий для обработки за один раз*/
#define LEN_NAME 16 /*Будем считать, что длина имени файла не превышает 16 символов*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*размер структуры события*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME )) /*буфер для хранения данных о событиях*/

//====================================================================================================//

int buf_i;

char* Concatinate(char *part1, char *part2);
int GetFileSize(int fd);
int CopyFile(char *path_out, char *path_to);
int ArrEqual(char *arr1, char *arr2);
int RemoveDirectory(char *path);
int RemoveExtra(char *path_from, char *path_to);
int DifferentFiles(char * path_1, char * path_2);
int CopyDir(char *path_out, char *path_to);
void Mainloop();
char * Find_command(char * txt);

//====================================================================================================//

int main(int argc, char ** argv) {
	if (argc < 3){
		printf("It must be more than 2 arguments\n");
		exit(EXIT_FAILURE);
	}

	if ((argc == 4) && (!strcmp("-auto", argv[3]))){
		int pid = fork();
		switch(pid) {
		case 0:
			//setsid();
			printf("CHILD NOT DAEMON\n");
			Mainloop();
			exit(0);
		case -1:
			printf("Fail: unable to fork\n");
			break;
		default:
			printf("OK: demon with pid %d is created\n", pid);
			int wst;
			wait(&wst);
			break;
		}
	}

	if (argc == 3){
		CopyDir(argv[1], argv[2]);
	}

	return 0;
}

//====================================================================================================//

void Mainloop(){
	printf("DAEMON: already daemon\n");

	int ret = 0;

	char * command;
	char text[PATH_MAX] = {0};

	int fd_chanel = open("chanel.txt", O_RDWR | O_CREAT, 0666);
	if(fd_chanel == -1){
		perror("DAEMON: open return -1\n");
	}

	printf("DAEMON: before while(1)\n");

	int CCCCounter = 0;

	int ino_chanel = inotify_init();
	inotify_add_watch(ino_chanel, "chanel.txt", IN_CLOSE | IN_OPEN | IN_MODIFY);
	char buf[sizeof(struct inotify_event) + PATH_MAX];

	while(1){

		printf("DAEMON: in begining while(1)\n");

		while (read(ino_chanel, (void *) buf, PATH_MAX) <= 0) {
			;
		}

		printf("DAEMON: event happens\n");

		memset(text, '\0', PATH_MAX);
		command = text;

		lseek(fd_chanel, SEEK_SET, 0);
		ret = read(fd_chanel, text, PATH_MAX);

		printf("DAEMON: read %d\n", ret);

		command = Find_command(text);

		printf("DAEMON: GET %s\n", text);
		printf("DAEMON: FIND %s\n", command);

		if (!command){
			continue;
		}
		
		//close(fd_chanel);
		//remove("chanel.txt");
		int fd_chanel = open("chanel.txt", O_RDWR | O_CREAT, 0666);
		if(fd_chanel == -1){
			perror("DAEMON: open return -1\n");
		}
		

		if(!strncmp(command, "bcp_dir", 4)){
			dprintf(fd_chanel, "DAEMON: get command: bcp_dir\n");
			dprintf(fd_chanel, "DAEMON: I do nothing)\n");
		}else if(!strncmp(command, "cpy_dir", 4)){
			dprintf(fd_chanel, "DAEMON: get command: cpy_dir\n");
			dprintf(fd_chanel, "DAEMON: I do nothing)\n");
		}else if(!strncmp(command, "log", 3)){
			printf("DAEMON: get command: log\n");
			dprintf(fd_chanel, "DAEMON: get command: log\n");
			dprintf(fd_chanel, "DAEMON: I do nothing)\n");
		}else if(!strncmp(command, "auto", 4)){
			dprintf(fd_chanel, "DAEMON: get command: auto\n");
			dprintf(fd_chanel, "DAEMON: I do nothing)\n");
		}else if(!strncmp(command, "backup", 4)){
			dprintf(fd_chanel, "DAEMON: get command: backup\n");
			dprintf(fd_chanel, "DAEMON: I do nothing)\n");
		}else if(!strncmp(command, "exit", 4)){
			dprintf(fd_chanel, "DAEMON: get command: exit\n");
			dprintf(fd_chanel, "DAEMON: This file will be removed, please leave without saving\n");
			close(fd_chanel);
			remove("chanel.txt");
			exit(EXIT_SUCCESS);
		}else if(!strncmp(command, "term", 4)){
			dprintf(fd_chanel, "DAEMON: get command: term\n");
			dprintf(fd_chanel, "DAEMON: I do nothing)\n");
		}else{
			dprintf(fd_chanel, "DAEMON: get command: unknown command\n");
			dprintf(fd_chanel, "DAEMON: I do nothing)\n");
		}

	}
}

//====================================================================================================//

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
				printf("Files are different\n");
				CopyFile(new_path_from, new_path_to);
			}else{
				printf("Files are equal\n");
			}			
			break;
		case DT_FIFO:
			printf("Get fifo file, skip\n");
			break;
		case DT_LNK:
			printf("Get link, skip\n");
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

char* Concatinate(char *part1, char *part2){
	char* result = (char *) calloc(256, sizeof (char));
	buf_i = 0;
	while (part1[buf_i] != 0){
		result[buf_i] = part1[buf_i];
		buf_i++;
		if(buf_i == 255){
			free(result);
			return NULL;
		}
	}
	result[buf_i] = '/';
	buf_i++;
	if(buf_i == 255){
		free(result);
		return NULL;
	}
	int len_part_1 = buf_i;
	while (part2[buf_i - len_part_1] != 0){
		result[buf_i] = part2[buf_i - len_part_1];
		buf_i++;
		if(buf_i == 255){
			free(result);
			return NULL;
		}
	}
	//result[buf_i] = '/';
	return result;
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
	printf("CopyFile%s\n", path_out);
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
		it_exist = 0;
		while ((dt_from = readdir(pdir_from)) != NULL){
			if(ArrEqual(dt_to->d_name, dt_from->d_name)){
				it_exist++;
				break;
			}
		}
		if(!it_exist){ //значит удаляем
			char* new_path = Concatinate(path_to, dt_to->d_name);
			if(dt_to->d_type == DT_DIR){
				RemoveDirectory(new_path);
			}else{
				printf("Unlinl %s\n", new_path);
				unlink(new_path);
			}
			free(new_path);
		}
		rewinddir(pdir_from);
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
		printf("comparator returning cause does not exist ERRNO = %i\n", errno);
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