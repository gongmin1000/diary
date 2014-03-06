#include <stdio.h>
#include <string.h>
void Add(char* k,char* l){
    char a = *k ;
    char b = *l;
    printf("in = %d,%d\n",a,b);
    a += b;
    printf("a1 = %d\n",a);
    if( a > 9 ){
        a -= 10;
        *k = a;
        printf("a11 = %d\n",*k);
        char c = 1;
        k++;
        Add(k,&c);
    }
    else{
        printf("a2 = %d\n",a); 
        *k = a;
    }
}

void AddNum(char* j,char* k,char* l){
    char a[1024];
    char b[1024];
    memset(a,0,sizeof(a));
    memset(b,0,sizeof(b));
    int lena = strlen(j);
    int lenb = strlen(k);
    char* q;
    char* w;
    q = j;
    w = k;
    if( lena < lenb ){
        q = k;
        w = j;
        int r = lena;
        lena = lenb;
        lenb = r;
    }
    int i,i2;
    for(i = (lena - 1),i2 = 0; i >= 0;i--,i2++ ){
        a[i2] = q[i] - 48;
    }
    for(i = (lenb - 1),i2 = 0; i >= 0;i--,i2++ ){
        b[i2] = w[i] - 48;
    }
    for(i = 0; i < lena ;i++ ){
        Add(&a[i],&b[i]);     
    }
    lena = 100;
    printf("lena = %d\n",lena);
    for( i = (lena - 1); i >= 0 ; i-- ){
        a[i] += 48;
        printf("%c",a[i]);
    }
    printf("\n");
}

int main(int argc,char* argv[]){
   char a[1024];
   char b[1024];
   char c[1024];
   memset(c,0,sizeof(c));
   strcpy(a,"999999999999");
   strcpy(b,"1");
   AddNum(a,b,c);
   printf("c = %s\n",c);
}

