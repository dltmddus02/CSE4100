/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
// #include <sys/time.h>

void echo(int connfd);

typedef struct{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
}pool;

struct item{
    int ID; 
    int left_stock; // 잔여 주식
    int price; // 주식 단가
    int readcnt;
    sem_t mutex;
};

typedef struct node{
    struct item data;
    struct node *left, *right;
} node;
node *tree;

void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
void tree_insert(node **tree, int ID, int left_stock, int price);
void buy_tree(node *tree, int ID, int stock_num, int connfd);
void sell_tree(node *tree, int ID, int stock_num, int connfd);
void show_tree(node *tree, char *show_cmdline);
void free_tree(node *tree);

node *search_node(node **tree, int ID);

int main(int argc, char **argv) 
{
    // struct timeval start, end;
    // unsigned long e_usec;
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

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

    // gettimeofday(&start, 0);

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("hostname : %s, port : %s\n", client_hostname, client_port);
            add_client(connfd, &pool);
        }
       check_clients(&pool);
    }
    // tree free시키기
    free_tree(tree);
    // gettimeofday(&end, 0);

    // printf("실행\n");
    // e_usec = ((end.tv_sec * 1000000) + end.tv_usec) - ((start.tv_sec * 1000000) + start.tv_usec);
    
    // printf("elapsed time: %lu microseconds\n", e_usec);
    exit(0);
}   
/* $end echoserverimain */

void init_pool(int listenfd, pool *p)
{
	/* Initially, there are no connected descriptors */
	int i;
	p -> maxi = -1;
	for (i = 0; i < FD_SETSIZE; i++)
		p -> clientfd[i] = -1;

	/* Initially, listenfd is only member of select read set */
	p -> maxfd = listenfd;
	FD_ZERO(&p -> read_set);
	FD_SET(listenfd, &p -> read_set);
}

void add_client(int connfd, pool *p)
{
	int i;
	p -> nready--;
	for (i = 0; i < FD_SETSIZE; i++){ /* Find an available slot */
		if (p -> clientfd[i] < 0) {
			/* Add connected descriptor to the pool */
			p -> clientfd[i] = connfd;
			Rio_readinitb(&p -> clientrio[i], connfd);

			/* Add the descriptor to descr iptor set */
			FD_SET(connfd, &p -> read_set);

			/* Update max descriptor and pool high water mark */
			if (connfd > p -> maxfd)
				p -> maxfd = connfd;
			if (i > p -> maxi)
				p -> maxi = i;
			break;
		}
	}
	if (i == FD_SETSIZE) /* Couldn't find an empty slot */
		app_error("add_client error : Too many clients");
}

void check_clients(pool *p)
{
    int i, connfd, n;
    char buf[MAXLINE];
    char cmdline[MAXLINE];
    rio_t rio;
    for(i = 0; (i <= p -> maxi) && (p -> nready > 0); i++) {
        connfd = p-> clientfd[i];
        rio = p-> clientrio[i];

        /* If the descriptor is ready, echo a text line from it */
		if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p -> nready--;
            // 한 줄에 대한 echo
            if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                strcpy(cmdline, buf);
                cmdline[n-1] = '\0';
                printf("Server received %dbytes on fd %d\n", n, connfd);
                if(strcmp(cmdline, "show") == 0){
                    char show_cmdline[MAXLINE] = "";
                    show_tree(tree, show_cmdline);
                    Rio_writen(connfd, show_cmdline, MAXLINE);
                }
                else if(strcmp(cmdline, "exit") == 0){
                    Close(connfd);
                    FD_CLR(connfd, &p -> read_set);
                    p -> clientfd[i] = -1;
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
                else {
                    Close(connfd);
                    FD_CLR(connfd, &p -> read_set);
                    p -> clientfd[i] = -1;
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
            }
            else{
                Close(connfd);
                FD_CLR(connfd, &p -> read_set);
                p -> clientfd[i] = -1;
            }
        }
    }
}

void tree_insert(node **tree, int ID, int left_stock, int price){
    node *new_node = (node*)malloc(sizeof(node));
    new_node -> data.ID = ID;
    new_node -> data.left_stock = left_stock;
    new_node -> data.price = price;
    new_node -> left = NULL;
    new_node -> right = NULL;

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
        cur->data.left_stock -= stock_num;
        Rio_writen(connfd, "[buy] success\n", MAXLINE);
    }
}

void sell_tree(node *tree, int ID, int stock_num, int connfd){
    node *cur = search_node(&tree, ID);
    if (cur == NULL){
        Rio_writen(connfd, "[sell] fail\n", MAXLINE);
    }
    else if (cur != NULL) {
        cur->data.left_stock += stock_num;
        Rio_writen(connfd, "[sell] success\n", MAXLINE);
    }
}

void show_tree(node *tree, char *show_cmdline)
{
    char temp[MAXLINE] = "";
    
    if(tree == NULL) {
        return;
    }
    show_tree(tree -> left, show_cmdline);
    sprintf(temp, "%d %d %d\n", tree -> data.ID, tree -> data.left_stock, tree -> data.price);
    strcat(show_cmdline, temp);
    show_tree(tree -> right, show_cmdline);
}

void free_tree(node *tree){
    if(tree == NULL) return;
    free_tree(tree->left);
    free_tree(tree->right);

    free(tree);
}