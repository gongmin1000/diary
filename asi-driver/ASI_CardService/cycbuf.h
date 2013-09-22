#ifndef CYCBUF_H
#define CYCBUF_H

class CycBuf
{
public:
    CycBuf(unsigned int buf_size);
    ~CycBuf();
    unsigned int AddData(unsigned char* buf,int size);
    unsigned int GetData(unsigned char* buf,int size);
private:
    unsigned int _buf_size;
    unsigned char* _buf;
    unsigned char* _buf_end;
    unsigned char* _start;
    unsigned char* _end;

};

#endif // CYCBUF_H
