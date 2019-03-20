#ifndef __RECORDS_H_INCLUDED__
#define __RECORDS_H_INCLUDED__

struct String
{
    char* chars;
};

struct Record
{
    struct String name;
    struct String number;
};

struct Records
{
    struct Record record;
    struct Records* next;
};

#endif
