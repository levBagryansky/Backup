#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>

//====================================================================================================//

int inotify_mode = 0;
int buf_i;

void PrintEvent(struct inotify_event *event);
char* Concatinate(char *part1, char *part2);
int GetFullPath(char* path, char *full_path); // Переводит путь к файлу в полный путь
int DestInSource(char *destination, char *source); //return 1 if destination in source, 0 if not and -1 if destination doesn't exist, -2 if src
int GetFileSize(int fd);
int CopyFile(char *path_out, char *path_to);
int ArrEqual(char *arr1, char *arr2);
int RemoveDirectory(char *path);
int RemoveExtra(char *path_from, char *path_to);
int DifferentFiles(char * path_1, char * path_2);
int CopyDir(char *path_out, char *path_to);
void mainloop();
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

//====================================================================================================//

int main(int argc, char ** argv) {
	if (argc < 3){
		printf("It must be more than 2 arguments\n");
		exit(EXIT_FAILURE);
	}
	int dst_in_str = DestInSource(argv[2], argv[1]);
	if(dst_in_str){
		switch (dst_in_str) {
			case 1:
				printf("destination in source\n");
				exit(EXIT_FAILURE);
				break;
			case -1:
				printf("destination does not exist\n");
				break;
			case -2:
				printf("source does not exists\n");
				exit(EXIT_FAILURE);
				break;
		}
	}

	if ((argc == 4) && (!strcmp("-auto", argv[3]))){
		//printf("It must running demon, but not yet)\n");
		//printf("%d\n", PATH_MAX);
		//char cwd[PATH_MAX];
		mkfifo("chanel" , O_RDWR | 0777);
		int pid = fork();
		switch(pid) {
		case 0:
			printf("Hey\n");
			setsid();
			printf("Im alive\n");
			mainloop();
			exit(0);
		case -1:
			printf("Fail: unable to fork\n");
			break;
		default:
			printf("OK: demon with pid %d is created\n", pid);
			break;
		}
		return 0;
	}

	if ((argc == 4 || argc == 5) && (!strcmp(argv[3], "-inotify") || !strcmp(argv[4], "-inotify"))){
		inotify_mode++;
		int forked = 0;
		if(forked == 0) {
			//setsid();
			int ino_fd = inotify_init();
			CopyDir(argv[1], argv[2]);
			SetInotifyRecursively(argv[1], ino_fd);
			char buf[sizeof(struct inotify_event) + PATH_MAX];
			while (1){
				int i = 1;
				while (read(ino_fd, (void *) buf, PATH_MAX) <= 0) {
					;
				}
				PrintEvent((struct inotify_event*) buf);
				RemoveExtra(argv[1], argv[2]);
				CopyDir(argv[1], argv[2]);
				close(ino_fd);
				ino_fd = inotify_init();
				SetInotifyRecursively(argv[1], ino_fd);
				//while (read(ino_fd, (void *) buf, PATH_MAX) <= 0) {;}
			}
		} else{
			printf("Deamon has been run\n");
		}
	}

	if (argc == 3){
		RemoveExtra(argv[1], argv[2]);
		CopyDir(argv[1], argv[2]);
	}

	return 0;
}

//====================================================================================================//

void mainloop(){
	printf("Still here\n");

	char command[PATH_MAX] = {0};

	int chanel = open("chanel", O_RDWR);
	if(chanel == -1){
		perror("Can't open fifo chanel\n");
	}

	dprintf(chanel, "HELLO\n");

	while(1){
		read(chanel, command, 4096);
		dprintf(chanel, "DAEMON: I read %s\n", command);

		if (!strncmp(command, "bcp_dir", 4)){dprintf(chanel, "get command: bcp_dir\n");}
		else if (!strncmp(command, "cpy_dir", 4)){dprintf(chanel, "get command: bcp_dir\n");}
		else if (!strncmp(command, "log", 3)){dprintf(chanel, "get command: bcp_dir\n");}
		else if (!strncmp(command, "auto", 4)){dprintf(chanel, "get command: bcp_dir\n");}
		else if (!strncmp(command, "backup", 4)){dprintf(chanel, "get command: bcp_dir\n");}
		else if (!strncmp(command, "exit", 4)){dprintf(chanel, "get command: bcp_dir\n");}
		else if (!strncmp(command, "term", 4)){dprintf(chanel, "get command: bcp_dir\n");}
		else {dprintf(chanel ,"get command: unknown command\n");}

	}

	printf("HOW\n");
}

//====================================================================================================//

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

void PrintEvent(struct inotify_event *event){
	printf("In PrintEvent, ");
	if (event->mask & IN_CREATE){
		printf("%s in create\n", event->name);
	}
	if (event->mask & IN_DELETE){
		printf("%s in delete\n", event->name);
	}
	if (event->mask & IN_CLOSE){
		printf("%s in close\n", event->name);
	}
	if (event->mask & IN_MODIFY){
		printf("%s in modify\n", event->name);
	}
	if (event->mask & IN_CLOSE){
		printf("%s in close\n", event->name);
	}
}

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