#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#define TRY(fun, ...) do { \
    if (R_FAILED(res = fun(__VA_ARGS__))) { \
        printf("fun: %08lX\n", res); \
        goto end; \
    } \
} while (0)

Result http_download(const char *url, uint32_t *len, uint8_t **buf) {
    *buf = NULL;

    Result res;

    printf("Downloading %s\n", url);

    httpcContext context;
    TRY(httpcOpenContext, &context, HTTPC_METHOD_GET, url, 1);

    TRY(httpcSetSSLOpt, &context, SSLCOPT_DisableVerify);
    TRY(httpcAddRequestHeaderField, &context, "User-Agent", "lumadl");

    TRY(httpcBeginRequest, &context);

    uint32_t statuscode;
    TRY(httpcGetResponseStatusCodeTimeout, &context, &statuscode, 15ULL * 1000 * 1000 * 1000);
    printf("Request returned status code %lu\n", statuscode);

    if (statuscode != 200) {
        // BCD-encode the status code to make it human readable in the hex representation
        res = 0;
        while (statuscode)
        {
            res <<= 4;
            res |= statuscode % 10;
            statuscode /= 10;
        }
        res |= 0xFFFF0000;

        goto end;
    }

    *buf = malloc(0x1000);
    if (*buf == NULL) {
        printf("Failed to allocate memory\n");
        res = -1;
        goto end;
    }

    *len = 0;
    do {
        uint32_t read;
        res = httpcDownloadData(&context, *buf + *len, 0x1000, &read);
        *len += read;

        if (*len > 1 * 1024 * 1024) {
            printf("Downloaded more than one megabyte\n");
            res = -1;
            goto end;
        }

        uint8_t *newbuf = realloc(*buf, *len + 0x1000);
        if (newbuf == NULL) {
            printf("Failed to allocate memory\n");
            res = -1;
            goto end;
        }
        *buf = newbuf;
    } while (res == HTTPC_RESULTCODE_DOWNLOADPENDING);

    if (R_FAILED(res)) {
        printf("httpcDownloadData: %08lX", res);
        goto end;
    }

    printf("Downloaded %lu bytes\n", *len);

end:
    if (R_FAILED(res) && *buf != NULL) {
        free(*buf);
    }

    httpcCloseContext(&context);
    return res;
}

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    httpcInit(0);

    uint32_t len;
    uint8_t *buf;
    Result http_res = http_download("https://files.wank.party/luma_latest.firm", &len, &buf);

    httpcExit();

    if (!R_FAILED(http_res)) {
        printf("Writing boot.firm\n");

        FILE *firm = fopen("sdmc:/boot.firm", "w");
        if (firm != NULL) {
            fwrite(buf, len, 1, firm);
            fclose(firm);
            printf("Done!\n");
        } else {
            printf("fopen: %s\n", strerror(errno));
        }
    }

    printf("Press Start to exit...\n");
    while (aptMainLoop()) {
        gspWaitForVBlank();
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START)
            break;
    }
}
