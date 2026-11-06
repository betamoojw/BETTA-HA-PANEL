/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include "xiaozhi/xiaozhi_text.h"

int main(void)
{
    /* ASCII, two-, three- and four-byte UTF-8, at every capacity boundary. */
    const char *text = "A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80Z";
    const unsigned lengths[] = {0,0,1,1,3,3,3,6,6,6,6,10,11,11};
    for (unsigned size=0;size<sizeof(lengths)/sizeof(lengths[0]);++size) {
        char out[16]; memset(out,0x5a,sizeof(out));
        xiaozhi_text_copy(out,size,text);
        if (size) assert(strlen(out)==lengths[size]&&!memcmp(out,text,lengths[size]));
        for (unsigned i=size;i<sizeof(out);++i) assert(out[i]==0x5a);
    }
    char long_reply[700], out[512];
    for (unsigned i=0;i<699;i+=3) memcpy(long_reply+i,"\xe4\xb8\xad",3);
    long_reply[699]=0;
    xiaozhi_text_copy(out,sizeof(out),long_reply);
    assert(strlen(out)==510&&!memcmp(out,long_reply,510));
    xiaozhi_text_copy(out,sizeof(out),"");assert(!out[0]);
    puts("AI Assistant text: UTF-8 boundaries, long Chinese reply and destination guards passed");
}
