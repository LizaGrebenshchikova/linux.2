#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "records.h"

MODULE_LICENSE("GPL");

static int deviceOpen(struct inode* inode, struct file* file);
static int deviceRelease(struct inode* inode, struct file* file);
static ssize_t deviceRead(struct file* flip, char* buffer, size_t len, loff_t* offset);
static ssize_t deviceWrite(struct file* flip, const char* buffer, size_t len, loff_t* offset);

struct file_operations fops =
{
	.read = deviceRead,
	.write = deviceWrite,
	.open = deviceOpen,
	.release = deviceRelease
};

int majorNum;

struct Records* recordsHead = NULL;
struct Records* recordsTail = NULL;

static int __init init(void)
{
    majorNum = register_chrdev(0, "task1", &fops);
    if (majorNum < 0)
    {
        printk(KERN_ALERT "Could not register device: %d\n", majorNum);
        return majorNum;
    }
    else
    {
        printk(KERN_INFO "task1 device major number %d\n", majorNum);
        return 0;
    }
}

void clearRecords(void)
{
	while (recordsHead != NULL) {	
		kfree(recordsHead->record.name.chars);
		kfree(recordsHead->record.number.chars);
		kfree(recordsHead);

		recordsHead = recordsHead->next;
	}
	recordsTail = NULL;
}

static void __exit exit(void)
{
    unregister_chrdev(majorNum, "task1");
    clearRecords();
}

#define COMMAND_MAX_SIZE 1023
char command[COMMAND_MAX_SIZE];
size_t curCommandIdx = 0;

char response[2 * COMMAND_MAX_SIZE];
size_t curResponseIdx = 0;
size_t curResponseSize = 0;

static int deviceOpen(struct inode* inode, struct file* file)
{
    try_module_get(THIS_MODULE);
    return 0;
}

static int deviceRelease(struct inode* inode, struct file* file)
{
    module_put(THIS_MODULE);
    return 0;
}

void addRecord(size_t numberStart, size_t numberEnd, size_t nameStart, size_t nameEnd)
{
    struct Record newRecord;
    newRecord.number.chars = (char*) kmalloc(numberEnd - numberStart + 1, GFP_KERNEL);
    memcpy(newRecord.number.chars, command + numberStart, numberEnd - numberStart);
    newRecord.number.chars[numberEnd - numberStart] = '\0';
    newRecord.name.chars = (char*) kmalloc(nameEnd - nameStart + 1, GFP_KERNEL);
    memcpy(newRecord.name.chars, command + nameStart, nameEnd - nameStart);
    newRecord.name.chars[nameEnd - nameStart] = '\0';
    
    struct Records* node = (struct Records*) kmalloc(sizeof(struct Records), GFP_KERNEL);
    node->next = NULL;
    node->record = newRecord;
    if (recordsHead == NULL)
    {
        recordsHead = recordsTail = node;
    }
    else
    {
        recordsTail = recordsTail->next = node;
    }
}

#define NOT_FOUND "person not found\n"

void addString(const char* string)
{
    while (*string)
    {
        response[curResponseSize++] = (*string++);
    }
}

void addResponse(const struct Record* record)
{
    addString(record->name.chars);
    addString(" ");
    addString(record->number.chars);
    addString("\n");
}

char tmp[COMMAND_MAX_SIZE + 1];
void addNotFoundResponse(size_t nameStart, size_t nameEnd)
{
    memcpy(tmp, command + nameStart, nameEnd - nameStart);
    tmp[nameEnd - nameStart] = '\0';
    addString(tmp);
    addString(" ");
    addString(NOT_FOUND);
}

void findRecord(size_t nameStart, size_t nameEnd)
{
    struct Records* cur = recordsHead;
    while (cur != NULL)
    {
        if (memcmp(cur->record.name.chars, command + nameStart, nameEnd - nameStart) == 0)
        {
            addResponse(&cur->record);
            return;
        }
        cur = cur->next;
    }
    addNotFoundResponse(nameStart, nameEnd);
}

void removeRecord(size_t nameStart, size_t nameEnd)
{
    struct Records* cur = recordsHead;
	struct Records* prev = NULL;
    while (cur != NULL)
    {
        if (memcmp(cur->record.name.chars, command + nameStart, nameEnd - nameStart) == 0)
        {
			if (prev != NULL) {
				prev->next = cur->next;
			}

			if (cur->next == NULL) {
				recordsHead = recordsTail = NULL;
			}

			kfree(cur->record.name.chars);
			kfree(cur->record.number.chars);
			kfree(cur);
            return;
        }
		prev = cur;
        cur = cur->next;
    }
    addNotFoundResponse(nameStart, nameEnd);
}

void cleanFirstSymbols(size_t symbolCount)
{
    size_t i;
    for (i = symbolCount; i < curCommandIdx; ++i)
    {
        command[i - symbolCount] = command[i];
    }
    curCommandIdx -= (curCommandIdx > symbolCount ? symbolCount : curCommandIdx);
}

void cleanFirstCommand(void)
{
    size_t i;
    for (i = 0; i < curCommandIdx && command[i] != ';'; ++i);
    cleanFirstSymbols(i + 1);
}

#define isspace(c) ((c) == ' ' || (c) == '\n' || (c) == '\t')
void trimCommand(void)
{
    size_t symbolCount = 0;
    while (symbolCount < curCommandIdx && isspace(command[symbolCount]))
    {
        ++symbolCount;
    }
    if (symbolCount > 0)
    {
        cleanFirstSymbols(symbolCount);
    }
}

void tryExecuteCommand(void)
{
    trimCommand();
    if (curCommandIdx > 1 && (command[0] == 'a' || command[0] == 'f' || command[0] == 'r') && command[1] == ' ')
    {
        size_t numberStart = 2;
        size_t numberEnd = 2;
        size_t nameStart;
        size_t nameEnd;
        if (command[0] == 'a')
        {
            while (numberEnd < curCommandIdx && command[numberEnd] != ' ')
            {
                ++numberEnd;
            }
            nameStart = numberEnd + 1;
        }
        else
        {
            nameStart = 2;
        }
        nameEnd = nameStart;
        while (nameEnd < curCommandIdx && command[nameEnd] != ';')
        {
            ++nameEnd;
        }
        if (nameEnd >= curCommandIdx)
        {
            cleanFirstCommand();
            return;
        }
        if (command[0] == 'a')
        {
            addRecord(numberStart, numberEnd, nameStart, nameEnd);
        }
        else if (command[0] == 'f')
        {
            findRecord(nameStart, nameEnd);
        }
		else if (command[0] == 'r') {
			removeRecord(nameStart, nameEnd);
		}
    }
    cleanFirstCommand();
}

static ssize_t deviceRead(struct file* flip, char* buffer, size_t len, loff_t* offset)
{
    while (curCommandIdx > 0 && curResponseIdx >= curResponseSize)
    {
        tryExecuteCommand();
    }
    ssize_t bytesRead = 0;
    while (--len >= 0 && curResponseIdx < curResponseSize)
    {
        put_user(response[curResponseIdx++], buffer++);
        ++bytesRead;
    }

    if (curResponseIdx == curResponseSize)
    {
        curResponseSize = 0;
        curResponseIdx = 0;
    }

    return bytesRead;
}

static ssize_t deviceWrite(struct file* flip, const char* buffer, size_t len, loff_t* offset)
{
    if (curResponseSize > 0)
    {
        return 0;
    }
    size_t i;
    for (i = 0; i < len; ++i)
    {
        command[curCommandIdx] = buffer[i];
        if (++curCommandIdx == COMMAND_MAX_SIZE)
        {
            tryExecuteCommand();
        }
    }
    return len;
}

module_init(init);
module_exit(exit);


