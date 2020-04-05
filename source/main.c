#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <3ds.h>

void create_config() {
	char url[80];
	char token[80];
	FILE *file;
	static SwkbdState swkbd;

	printf("Creating config...\n");
	file = fopen("3dspost_config", "w");
	swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
	swkbdSetHintText(&swkbd, "Enter your instance URL...");
	swkbdInputText(&swkbd, url, sizeof(url));
	swkbdSetHintText(&swkbd, "Enter your OAuth token...");
	swkbdInputText(&swkbd, token, sizeof(token));
	fprintf(file, "%s\n%s", url, token);
	fclose(file);
}

// Currently unused
Result register_oauth_app(const char* url) {
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
	u32 statuscode=0;
	u32 contentsize=0, readsize=0, size=0;
	u8 *buf, *lastbuf;
	char reg_url[60];
	strcpy(reg_url, url);
	strcat(reg_url, "/api/v1/apps");
	printf("POSTing to %s \n", reg_url);

	do {
		ret = httpcOpenContext(&context, HTTPC_METHOD_POST, reg_url, 0);
		printf("return from httpcOpenContext: %" PRIx32 "\n",ret);

		// This disables SSL cert verification, so https:// will be usable
		ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
		printf("return from httpcSetSSLOpt: %" PRIx32 "\n",ret);

		// Enable Keep-Alive connections
		ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
		printf("return from httpcSetKeepAlive: %" PRIx32 "\n",ret);

		// Set a User-Agent header so websites can identify your application
		ret = httpcAddRequestHeaderField(&context, "User-Agent", "kawentexttranferprotocol/1.0.0");
		printf("return from httpcAddRequestHeaderField: %" PRIx32 "\n",ret);

		// Post specified data.
		ret = httpcAddPostDataAscii(&context, "client_name", "kawenapp");
		printf("return from httpcAddPostDataAsciii: %" PRIx32 "\n",ret);
		ret = httpcAddPostDataAscii(&context, "redirect_uris", "urn:ietf:wg:oauth:2.0:oob");
		printf("return from httpcAddPostDataAsciii: %" PRIx32 "\n",ret);
		ret = httpcAddPostDataAscii(&context, "scopes", "read write");
		printf("return from httpcAddPostDataAsciii: %" PRIx32 "\n",ret);

		ret = httpcBeginRequest(&context);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				return -1;
			}
			ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
			url = newurl; // Change pointer to the url that we just learned
			printf("redirecting to url: %s\n",url);
			httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

	if(statuscode!=200){
		printf("URL returned status: %" PRIx32 "\n", statuscode);
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -2;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return ret;
	}

	printf("reported size: %" PRIx32 "\n",contentsize);

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	do {
		// This download loop resizes the buffer as data is read.
		ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
		size += readsize; 
		if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
				lastbuf = buf; // Save the old pointer, in case realloc() fails.
				buf = realloc(buf, size + 0x1000);
				if(buf==NULL){ 
					httpcCloseContext(&context);
					free(lastbuf);
					if(newurl!=NULL) free(newurl);
					return -1;
				}
			}
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		return -1;
	}

	// Resize the buffer back down to our actual final size
	lastbuf = buf;
	buf = realloc(buf, size);
	if(buf==NULL){ // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	printf("response size: %" PRIx32 "\n",size);

	// Print result
	printf((char*)buf);
	printf("\n");
	
	gfxFlushBuffers();
	gfxSwapBuffers();

	httpcCloseContext(&context);
	free(buf);
	if (newurl!=NULL) free(newurl);

	return 0;
}

Result http_post(const char* url, const char* data, const char* token)
{
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
	u32 statuscode=0;
	u32 contentsize=0, readsize=0, size=0;
	u8 *buf, *lastbuf;
	char post_url[60];

	strcpy(post_url, url);
	strcat(post_url, "/api/v1/statuses");

	printf("POSTing %s\n", post_url);

	do {
		ret = httpcOpenContext(&context, HTTPC_METHOD_POST, post_url, 0);
		printf("return from httpcOpenContext: %" PRIx32 "\n",ret);

		// This disables SSL cert verification, so https:// will be usable
		ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
		printf("return from httpcSetSSLOpt: %" PRIx32 "\n",ret);

		// Enable Keep-Alive connections
		ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
		printf("return from httpcSetKeepAlive: %" PRIx32 "\n",ret);

		// Set a User-Agent header so websites can identify your application
		ret = httpcAddRequestHeaderField(&context, "User-Agent", "kawentexttranferprotocol/1.0.0");
		printf("return from httpcAddRequestHeaderField: %" PRIx32 "\n",ret);

		// Set authorization header
		char bearer[80];
		strcpy(bearer, "Bearer ");
		strcat(bearer, token);
		ret = httpcAddRequestHeaderField(&context, "Authorization", bearer);
		printf("return from httpcAddRequestHeaderField: %" PRIx32 "\n",ret);
		printf("auth header is %s\n", bearer);

		// Post specified data.
		ret = httpcAddPostDataAscii(&context, "status", data);
		printf("return from httpcAddPostDataAsciii: %" PRIx32 "\n",ret);

		ret = httpcBeginRequest(&context);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				return -1;
			}
			ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
			url = newurl; // Change pointer to the url that we just learned
			printf("redirecting to url: %s\n",url);
			httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

	if(statuscode!=200){
		printf("URL returned status: %" PRIx32 "\n", statuscode);
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -2;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return ret;
	}

	printf("reported size: %" PRIx32 "\n",contentsize);

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	do {
		// This download loop resizes the buffer as data is read.
		ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
		size += readsize; 
		if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
				lastbuf = buf; // Save the old pointer, in case realloc() fails.
				buf = realloc(buf, size + 0x1000);
				if(buf==NULL){ 
					httpcCloseContext(&context);
					free(lastbuf);
					if(newurl!=NULL) free(newurl);
					return -1;
				}
			}
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		return -1;
	}

	// Resize the buffer back down to our actual final size
	lastbuf = buf;
	buf = realloc(buf, size);
	if(buf==NULL){ // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	printf("response size: %" PRIx32 "\n",size);

	// Print result
	printf((char*)buf);
	printf("\n");
	
	gfxFlushBuffers();
	gfxSwapBuffers();

	httpcCloseContext(&context);
	free(buf);
	if (newurl!=NULL) free(newurl);

	return 0;
}

int main()
{
	Result ret=0;
	gfxInitDefault();
	httpcInit(4 * 1024 * 1024); // Buffer size when POST/PUT.
	char url[80];
	char token[80];

	consoleInit(GFX_TOP,NULL);

	FILE *file = fopen("3dspost_config", "rb");
	if (file == NULL) {
		printf("Looks like the configuration file doesn't exist\n");
		printf("Do you want to create it?\n");
		printf("A = Yes, B = No\n");
		while(true) {
			hidScanInput();
			u32 kDown = hidKeysDown();

			if (kDown & KEY_A) {
				create_config();
				break;
			}
			if (kDown & KEY_B) {
				break;
			}
		}
	}

	fgets(url, sizeof(url), file);
	fgets(token, sizeof(token), file);
	strtok(url, "\n");

	printf("Hello\n");
	printf("Press A to post\n");
	printf("Press Y recreate config\n");
	printf("Press START to exit\n");
	printf("Current instance URL is %s\n", url);
	printf("Current oauth token is %s\n", token);

	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		static SwkbdState swkbd;
		static char mybuf[60];
		SwkbdButton button = SWKBD_BUTTON_NONE;

		if (kDown & KEY_Y) {
			create_config();
		}

		if (kDown & KEY_A) {
			swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 3, -1);
			swkbdSetInitialText(&swkbd, mybuf);
			button = swkbdInputText(&swkbd, mybuf, sizeof(mybuf));
			if (button == SWKBD_BUTTON_RIGHT) {
				ret=http_post(url, mybuf, token);
				printf("return from http_post: %" PRIx32 "\n",ret);
			} else if (button == SWKBD_BUTTON_LEFT) {
				printf("Input cancelled\n");
			} else {
				printf("i don't understand this button\n");
			}
		}

		gfxFlushBuffers();
		gfxSwapBuffers();

		gspWaitForVBlank();
	}

	// Exit services
	httpcExit();
	gfxExit();
	return 0;
}
