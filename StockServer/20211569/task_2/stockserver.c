/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define SBUFSIZE 128
#define NTHREADS 128
void echo(int connfd);

typedef struct{
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
}sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

struct item{
    int ID; 
    int left_stock; // 잔여 주식
    int price; // 주식 단가
    int readcnt;
    sem_t mutex;
    sem_t writer;
};

typedef struct node{
    struct item data;
    struct node *left, *right;
} node;
node *tree;

static int byte_cnt;
static sem_t mutex;
sbuf_t sbuf;

void *thread(void *vargp);
static void init_echo_cnt(void);
void echo_cnt(int connfd);
void tree_insert(node **tree, int ID, int left_stock, int price);
node *search_node(node **tree, int ID);
void buy_tree(node *tree, int ID, int stock_num, int connfd);
void sell_tree(node *tree, int ID, int stock_num, int connfd);
void show_tree(node *tree, char *show_cmdline);
void free_tree(node *tree);

int main(int argc, char **argv) 
{
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    FILE* fp=fopen("stock.txt","r");
    if(!fp){
        fprintf(stderr,"stock file does not exist.\n");
        exit(0);
    }  

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);    
    }

    int ID, left_stock, price;
    while(!feof(fp)){
        int a = fscanf(fp, "%d %d %d", &ID, &left_stock, &price);
        if(a!=3) {
            // fprintf(stderr, "인자 3개 아니야\n");
        }
        tree_insert(&tree, ID, left_stock, price);
    }
    fclose(fp);


    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    for(i=0; i<NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("hostname : %s, port : %s\n", client_hostname, client_port);
    }
    // tree free시키기
    free_tree(tree);
    exit(0);
}   
/* $end echoserverimain */

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);
        echo_cnt(connfd);
        Close(connfd);
    }
}

static void init_echo_cnt(void){
    Sem_init(&mutex, 0, 1);
    byte_cnt=0;
}

void echo_cnt(int connfd){
    int n;
    char buf[MAXLINE];
    char cmdline[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    Pthread_once(&once, init_echo_cnt);
    Rio_readinitb(&rio, connfd);
    while((n=Rio_readlineb(&rio, buf, MAXLINE))!=0){
        P(&mutex);
        byte_cnt += n;
        printf("thread %d received %d (%d total) bytes on fd %d\n", (int)pthread_self(), n, byte_cnt, connfd);
        strcpy(cmdline, buf);
        cmdline[n-1] = '\0';
        if(strcmp(cmdline, "show") == 0){
            char show_cmdline[MAXLINE] = "";
            show_tree(tree, show_cmdline);
            Rio_writen(connfd, show_cmdline, MAXLINE);
        }
        else if(strncmp(cmdline, "buy", 3) == 0){
            int ID, stock_num;
            char str[5];
            sscanf(cmdline, "%s %d %d", str, &ID, &stock_num);
            buy_tree(tree, ID, stock_num, connfd);
        }
        else if(strncmp(cmdline, "sell", 4) == 0){
            int ID, stock_num;
            char str[5];
            sscanf(cmdline, "%s %d %d", str, &ID, &stock_num);
            sell_tree(tree, ID, stock_num, connfd);
        }
        char update[MAXLINE] = "";
        show_tree(tree, update);
        // printf("%s\n", update);
        FILE* fp1=fopen("stock.txt","w");
        if(!fp1){
            fprintf(stderr,"stock file does not exist.\n");
            exit(0);
        }  
        fprintf(fp1, "%s", update);
        fclose(fp1);

        V(&mutex);
    }
}

void sbuf_init(sbuf_t *sp , int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                              /* Buffer holds max of n items */
    sp->front = sp->rear = 0;               /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);             /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);             /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);             /* Initially, buf has 0 items */
}

void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item) {
	P(&sp->slots);							/* Wait for available slot */
	P(&sp->mutex);							/* Lock the buffer */
	sp->buf[(++sp->rear) % (sp->n)] = item;	/* Insert the item */
	V(&sp->mutex);							/* Unlock the buffer */
	V(&sp->items);							/* Announce available item */
}
int sbuf_remove(sbuf_t *sp) {
	int item;
	P(&sp->items);							/* Wait for available item */
	P(&sp->mutex);							/* Lock the buffer */
	item = sp->buf[(++sp->front)%(sp->n)];	/* Remove the item */
	V(&sp->mutex);							/* Unlock the buffer */
	V(&sp->slots);							/* Announce available slot */
	return item;
}

void tree_insert(node **tree, int ID, int left_stock, int price){
    node *new_node = (node*)malloc(sizeof(node));
    new_node -> data.ID = ID;
    new_node -> data.left_stock = left_stock;
    new_node -> data.price = price;
    new_node -> left = NULL;
    new_node -> right = NULL;
    Sem_init(&(new_node -> data.mutex), 0, 1);
    Sem_init(&(new_node -> data.writer), 0, 1);

    if(*tree == NULL){ // tree가 비어 있을 때
        *tree = new_node;
        return;
    }
    node* current = *tree; // 현재 root node
    node* parent = NULL;
    while(current != NULL){
        parent = current;
        if (ID < current->data.ID){
            current = current->left;
        }
        else if (ID > current->data.ID){
            current = current->right;
        }
        else {
            free(new_node);
            return;
        }
    }
    
    if(ID < parent -> data.ID) parent -> left = new_node;
    else parent -> right = new_node;
}

node* search_node(node** tree, int ID) {
    while(1){
        if((*tree) == NULL || ID == (*tree) -> data.ID) break;
        if(ID < (*tree)->data.ID){
            (*tree) = ((*tree)->left);
        }
        else if(ID > (*tree)->data.ID){
            (*tree) = ((*tree)->right);
        }
        else return NULL;
    }    
    return (*tree);
}

void buy_tree(node *tree, int ID, int stock_num, int connfd){
    node *cur = search_node(&tree, ID);
    if(cur == NULL){
        Rio_writen(connfd, "[buy] fail\n", MAXLINE);
        return;
    }
    else if (cur->data.left_stock < stock_num){
        Rio_writen(connfd, "Not enough left stocks\n", MAXLINE);
    }
    else if (cur != NULL) {
        P(&(tree->data.writer));
        cur->data.left_stock -= stock_num;
        Rio_writen(connfd, "[buy] success\n", MAXLINE);
        V(&(tree->data.writer));
    }
}

void sell_tree(node *tree, int ID, int stock_num, int connfd){
    node *cur = search_node(&tree, ID);
    if (cur == NULL){
        Rio_writen(connfd, "[sell] fail\n", MAXLINE);
    }
    else if (cur != NULL) {
        P(&(tree->data.writer));
        cur->data.left_stock += stock_num;
        Rio_writen(connfd, "[sell] success\n", MAXLINE);
        V(&(tree->data.writer));
    }
}


void show_tree(node *tree, char *show_cmdline)
{
    char temp[MAXLINE] = "";
    
    if(tree == NULL) {
        return;
    }

    P(&(tree->data.mutex));
    tree->data.readcnt++;
    if(tree->data.readcnt == 1) P(&(tree->data.writer));
    V(&(tree->data.mutex));

    sprintf(temp, "%d %d %d\n", tree -> data.ID, tree -> data.left_stock, tree -> data.price);
    strcat(show_cmdline, temp);

    P(&(tree->data.mutex));
    tree->data.readcnt--;
    if(!tree->data.readcnt) V(&(tree->data.writer));
    V(&(tree->data.mutex));

    show_tree(tree -> left, show_cmdline);
    show_tree(tree -> right, show_cmdline);
}

void free_tree(node *tree){
    if(tree == NULL) return;
    free_tree(tree->left);
    free_tree(tree->right);

    free(tree);
}