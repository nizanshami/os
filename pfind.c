#include <dirent.h>
#include <sys/stat.h>
#include <threads.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

mtx_t init_thrd_lock;
mtx_t qlock;
cnd_t init_thrd_cv;
cnd_t qcv;
// -----------------------------queue--------------------------------------

struct Node{
    char *path;
    struct Node *next;
};
// start-the next item in the queue, end- last item in the line
struct Queue{
    struct Node *start, *end;
    int size;
    char *search_term;
    int count;
};

struct Queue *init_queue(char *search_term){
    struct Queue *q = (struct Queue *)malloc(sizeof(struct Queue));
    q->start = NULL;
    q->end = NULL;
    q->size = 0;
    q->search_term = search_term;
    return q;
}

struct Node *init_node(char *path){
    struct Node *node = (struct Node *)malloc(sizeof(struct Node));
    node->path = path;
    node->next = NULL;
    return node;
}

void enqueue(struct Queue *q, char *path){
    struct Node *new = init_node(path);
    if(q->end == NULL){
        q->end = new;
        q->start = new;
        q->size++;
        return;
    }
    q->end->next = new;
    q->end = new;
    q->size++;
}

char *dequeue(struct Queue *q){
    struct Node *tmp;
    char *result_path;
    if(q->size == 0){
        return NULL;
    }
    tmp = q->start;
    q->start = q->start->next;
    if(q->size == 1){
        q->end = NULL;
    }
    q->size--;
    result_path = tmp->path;
    free(tmp);
    return result_path;
}

void free_queue(struct Queue *q){
    while (q->size != 0){
        dequeue(q);
    }
    free(q);
    
}
static struct Queue *q;
//--------------------------------------------------------------------------------

int count_files(char *path, struct Queue *q){
    DIR *dir;
    int count = 0;
    char *new_path;
    struct dirent *dirent;
    struct stat s;
    dir = opendir(path);
    if(dir == NULL){
        printf("Directory %s: Permission denied.\n", path);
        return count;
    }

    while((dirent = readdir(dir)) != NULL){
        new_path = (char *)malloc(strlen(path) + strlen(dirent->d_name) + 2);
        sprintf(new_path, "%s/%s", path, dirent->d_name);
        stat(new_path, &s);        
        if(S_ISDIR(s.st_mode)){
            if(strcmp(dirent->d_name , ".") && strcmp(dirent->d_name , "..")){
                mtx_lock(&qlock);
                enqueue(q, new_path);
                cnd_signal(&qcv);
                mtx_unlock(&qlock);
            }else{
                free(new_path);
            }
        }else{
            if(strstr(dirent->d_name, q->search_term) != NULL){
                printf("%s\n", new_path);
                count++;
            }
            free(new_path);
            
        }
    }
    closedir(dir);
    return count;
}

int pick_dir(struct Queue *q){
    char *path;
    int count = 0;
    while(q->size > 0){
        mtx_lock(&qlock);
        if((path = dequeue(q)) == NULL){
            cnd_wait(&qcv, &qlock);
        }
        mtx_unlock(&qlock);
        count += count_files(path, q);
        free(path);
            
    }
    
    thrd_exit((int)count);
}

int init_thrd(void *t){
    mtx_lock(&init_thrd_lock);
    (*((int *)t))--; 
    cnd_wait(&init_thrd_cv, &init_thrd_lock);
    mtx_unlock(&init_thrd_lock);
    

    return pick_dir(q);    
}
//--------------------------------------------------------------------------------

int main(int argc, char *argv[]){
    char *search_term, *root;
    int n, rc, i, *t, thrd_files, sum_files = 0;
    DIR *dir;

    if(argc != 4){
        perror("not enough arguments");
        exit(1);
    }
    if((dir = opendir(argv[1])) == NULL){
        perror("root file can't be search");
        exit(1);
    }else{
        closedir(dir);
    }
    
    search_term = argv[2];
    root = (char *)malloc(strlen(argv[1])* sizeof(char) + 1);
    root = strcpy(root, argv[1]);
    n = atoi(argv[3]);
    
    q = init_queue(search_term);
    enqueue(q, root);

    rc = mtx_init(&init_thrd_lock, mtx_plain);
    if (rc != thrd_success) {
    perror("ERROR in mtx_init()");
    exit(1);
    }
    rc = cnd_init(&init_thrd_cv);
    if(rc != thrd_success){
        perror("ERROR in cnd_init()");
        exit(1);
    }
    rc = mtx_init(&qlock, mtx_plain);
    if (rc != thrd_success) {
    perror("ERROR in mtx_init()");
    exit(1);
    }
    rc = cnd_init(&qcv);
    if(rc != thrd_success){
        perror("ERROR in cnd_init()");
        exit(1);
    }


    thrd_t threads[n];
    t = (int *)malloc(sizeof(int));
    *t = n;

    for(i = 0; i < n;i++){
        rc = thrd_create(&threads[i], init_thrd, t);
        if(rc != thrd_success){
            perror("ERROR in thread create");
            exit(1);
        }
    }
    
    while (1){
        if(*t == 0){
            cnd_broadcast(&init_thrd_cv);
            break;
        }
        sleep(1);
    }

    while(q->size > 0){
        cnd_signal(&qcv);
    }
    
    //wait all the thread are created
    for(i = 0;i < n;i++){
        rc = 0;
        rc += thrd_join(threads[i], &thrd_files);
        if(rc == (n * thrd_error)){
            perror("ERROR: en error occurs in all threads");
            exit(1);
        }
        sum_files += thrd_files;
    }
    printf("Done searching, found %d files\n", sum_files);
    cnd_destroy(&init_thrd_cv);
    mtx_destroy(&init_thrd_lock);
    cnd_destroy(&qcv);
    mtx_destroy(&qlock);
    free_queue(q);
    free(t);
    return 0;
}