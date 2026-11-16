/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
static esp_err_t invoke(unsigned format)
{
    httpd_req_t req={0};
    return format==0 ? api_screenshot_png_get_handler(&req) : format==1 ? api_screenshot_jpg_get_handler(&req) : api_screenshot_bmp_get_handler(&req);
}
static void verify_pixels(unsigned format)
{
    uint8_t *rgb=malloc((size_t)display_w*display_h*3); CHECK(rgb);
    if (format==0) CHECK(decode_png(display_w,display_h,rgb));
    if (format==2) {
        size_t stride=((size_t)display_w*3+3)&~3U;
        CHECK(body_len==54+stride*display_h && body[0]=='B' && body[1]=='M');
        for(int y=0;y<display_h;y++) {
            const uint8_t *row=body+54+(display_h-1-y)*stride;
            for(int x=0;x<display_w;x++) for(unsigned c=0;c<3;c++) rgb[((size_t)y*display_w+x)*3+c]=row[x*3+2-c];
            for(size_t x=display_w*3;x<stride;x++) CHECK(row[x]==0);
        }
    }
    for(int y=0;y<display_h;y++) for(int x=0;x<display_w;x++) for(unsigned c=0;c<3;c++)
        CHECK(rgb[((size_t)y*display_w+x)*3+c]==expected(x,y,2-c));
    free(rgb);
}
static void large_cases(void)
{
    const int sizes[][2]={{3,2},{13,67},{1280,800},{800,1280}};
    for(unsigned f=0;f<3;f++) {
#if !SOC_JPEG_ENCODE_SUPPORTED
        if(f==1) { reset(); CHECK(invoke(f)==ESP_OK && !strcmp(status,"501 Not Implemented") && !heap_calls); continue; }
#endif
        for(unsigned s=0;s<4;s++) for(unsigned layer=0;layer<3;layer++) {
            reset(); display_w=sizes[s][0];display_h=sizes[s][1];popup=layer==1;opaque_cover=layer==2;
            CHECK(invoke(f)==ESP_OK && !status[0] && body_len); clean();
            CHECK(lock_calls==2 && unlock_calls==2 && largest_alloc<=1048576 && peak_bytes<=5169152);
            if(f!=1) verify_pixels(f);
            else CHECK(jpeg_calls==(display_h+31)/32 && body[body_len-2]==255 && body[body_len-1]==217);
        }
        reset(); display_w=1280;display_h=800;popup=true; CHECK(invoke(f)==ESP_OK); int count=heap_calls;
        /* Every application/zlib allocation ordinal, including partial band lists. */
        for(int i=1;i<=count;i++) {
            reset(); display_w=1280;display_h=800;popup=true;fail_alloc_at=i;
            CHECK(invoke(f)==ESP_OK && !strcmp(status,"500 Internal Server Error") && !body_len); clean();
        }
        const int failures_at[]={1,2,25,49,50};
        for(unsigned i=0;i<5;i++) {
            reset();display_w=1280;display_h=800;popup=true;fail_render_at=failures_at[i];
            CHECK(invoke(f)==ESP_OK && !strcmp(status,"500 Internal Server Error") && !body_len); clean();
        }
        reset();display_w=1280;display_h=800;opaque_cover=true;fail_render_at=25;
        CHECK(invoke(f)==ESP_OK && !strcmp(status,"500 Internal Server Error"));clean();
        for(int i=0;i<12;i++) {
            reset();popup=true; fail_send_at=i%2 ? 1 : 0;
            CHECK(invoke(f)==(fail_send_at ? ESP_FAIL : ESP_OK));clean();
        }
    }
    for(int at=1;at<=3;at++) {
        reset(); fail_deflate_at=at; CHECK(invoke(0)==ESP_OK && !strcmp(status,"500 Internal Server Error"));clean();
    }
    reset(); display_w=1280;display_h=800;noise=true;
    CHECK(invoke(0)==ESP_OK && !strcmp(status,"500 Internal Server Error") && !body_len);clean();
    const int send_at[]={1,2,3,6};
    for(unsigned i=0;i<4;i++) { reset();fail_send_at=send_at[i];CHECK(invoke(2)==ESP_FAIL);clean(); }
#if SOC_JPEG_ENCODE_SUPPORTED
    for(int error=0;error<5;error++) {
        reset();display_w=1280;display_h=800;
        if(error==0)fail_engine=1; else if(error==1)fail_jpeg_at=25; else bad_jpeg=error-1;
        CHECK(invoke(1)==ESP_OK && !strcmp(status,"500 Internal Server Error") && !body_len);clean();
    }
#endif
}
