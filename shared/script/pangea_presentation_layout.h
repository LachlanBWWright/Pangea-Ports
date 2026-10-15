#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

static inline size_t PangeaPresentation_UTF8Character(const unsigned char* text, size_t remaining, unsigned int* codepoint)
{
	if (!remaining) return 0;
	unsigned char first=text[0];
	size_t count=first<0x80 ? 1 : first>=0xc2 && first<=0xdf ? 2 : first>=0xe0 && first<=0xef ? 3 : first>=0xf0 && first<=0xf4 ? 4 : 0;
	if (!count || count>remaining) return 0;
	unsigned int value=first & (count==1 ? 0x7f : count==2 ? 0x1f : count==3 ? 0x0f : 0x07);
	for (size_t i=1; i<count; i++)
	{
		if ((text[i]&0xc0)!=0x80) return 0;
		value=(value<<6)|(text[i]&0x3f);
	}
	if ((count==2 && value<0x80) || (count==3 && value<0x800) || (count==4 && value<0x10000) ||
		(value>=0xd800 && value<=0xdfff) || value>0x10ffff) return 0;
	*codepoint=value;
	return count;
}

static inline bool PangeaPresentation_ValidUTF8(const char* text, size_t size)
{
	for (size_t position=0; position<size;)
	{
		unsigned int codepoint;
		size_t consumed=PangeaPresentation_UTF8Character((const unsigned char*)text+position,size-position,&codepoint);
		if (!consumed) return false;
		position+=consumed;
	}
	return true;
}

static inline void PangeaPresentation_Latin1(const char* text, char output[128])
{
	size_t remaining=strlen(text), position=0;
	while (remaining && position<127)
	{
		unsigned int codepoint;
		size_t consumed=PangeaPresentation_UTF8Character((const unsigned char*)text,remaining,&codepoint);
		if (!consumed) { consumed=1; codepoint='?'; }
		if (codepoint==0x2018 || codepoint==0x2019) codepoint='\'';
		if (codepoint==0x201c || codepoint==0x201d) codepoint='"';
		if (codepoint==0x2013 || codepoint==0x2014) codepoint='-';
		if (codepoint==0x2026) codepoint='.';
		output[position++]=(char)(codepoint<=255 ? codepoint : '?');
		text+=consumed; remaining-=consumed;
	}
	output[position]=0;
}

static inline size_t PangeaPresentation_WrapLine(const char* text, char output[128])
{
	size_t length=0, space=0;
	while (text[length] && text[length]!='\n' && length<42)
	{
		if (text[length]==' ') space=length;
		length++;
	}
	if (length==42 && text[length] && space>0) length=space;
	while (length>0 && ((unsigned char)text[length]&0xc0)==0x80) length--;
	if (!length && text[0] && text[0]!='\n' && text[0]!=' ') length=1;
	memcpy(output,text,length);
	output[length]=0;
	size_t consumed=length;
	while (text[consumed]==' ' || text[consumed]=='\n') consumed++;
	return consumed;
}
