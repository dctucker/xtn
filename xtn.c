#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include "xtnres.h"

#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <winsock.h>
#include <shellapi.h>

#define AddItem(item)	ListBox_AddString(GetDlgItem(hWndDlg,IDLIST),item)
#define SOCKET_READY	0x420
#define PRNERROR(er)	MessageBox(hWndDlg,er,er,0)
#define Dlg(item)		GetDlgItem(hWndDlg,item)
#define Send(item)		send(connection,item,strlen(item),0)
#define nullStr(item)	memset(item,0,sizeof(item))
#define Status(item)	SetDlgItemText(hWndDlg,IDSTATUS,item)
#define OnSignal()		WSAAsyncSelect(connection,hWndDlg,SOCKET_READY,FD_READ)
#define OffSignal()		WSAAsyncSelect(connection,hWndDlg,0,0)
#define Cursor(sC)		SetCursor(LoadCursor(NULL,MAKEINTRESOURCE(sC)));
#define ClearIDEDIT()	if( Edit_GetTextLength(Dlg(IDEDIT)) !=0) Edit_SetText(Dlg(IDEDIT),"");

#define _WIN32_IE 0x0500
/*
typedef struct _NOTIFYICONDATANEW {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HICON hIcon;
    TCHAR szTip[64];
    DWORD dwState;
    DWORD dwStateMask;
    TCHAR szInfo[256];
    union {
        UINT uTimeout;
        UINT uVersion;
    };
    TCHAR szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID guidItem;
} NOTIFYICONDATANEW, *PNOTIFYICONDATANEW;
*/
typedef struct LLIST {
	char data[128];
	struct LLIST *next;
} LLIST;
LLIST lPlaylist;

// prototype for the dialog box function.
static BOOL CALLBACK DialogFunc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

SOCKET	CallServer(LPCSTR lpServerName);
void	ParseCmd(LPCSTR str);
void	Case14(LPCSTR str);
void	Resize();
void	MoveWin(HWND h, int x, int y);
void	UpdateStatic(int vol);
void	Connect();
void	Disconnect();
void	LoadPlaylist();
void	FindSong();
void	ResetList();
BOOL	Matches(char *song,char *szdata);
const char	*String(char *format,...);
void	Balloon(char *Title, char *Message);

SOCKET		connection;
HWND		hWndDlg;
HINSTANCE	hInstDlg;
NOTIFYICONDATA nid;
BOOL		PosUpdate,
			FirstRun;

int APIENTRY WinMain(HINSTANCE hinst, HINSTANCE hinstPrev, LPSTR lpCmdLine, int nCmdShow){
	WNDCLASS wc;

	memset(&wc,0,sizeof(wc));
	wc.style=CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = DefDlgProc;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hInstance = hinst;
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.hIcon = LoadIcon(hinst,MAKEINTRESOURCE(IDAPPICON));

	wc.lpszClassName = "xtn";
	RegisterClass(&wc);

	hInstDlg=hinst;

	return DialogBox(hinst, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL, (DLGPROC) DialogFunc);
}

static int InitializeApp(HWND hDlg,WPARAM wParam, LPARAM lParam){
	WORD wVersionReq=MAKEWORD(1,1);
	WSADATA wsaData;
	int nRet;
	hWndDlg=hDlg;
	PosUpdate=TRUE;
	FirstRun=TRUE;
	connection=FALSE;
	lPlaylist.next=NULL;
	nullStr(lPlaylist.data);

	Button_SetImage(Dlg(IDBPREV),IDBITMAPPREV);
	Button_SetImage(Dlg(IDBPLAY),IDBITMAPPLAY);
	Button_SetImage(Dlg(IDBPAUSE),IDBITMAPPAUSE);
	Button_SetImage(Dlg(IDBSTOP),IDBITMAPSTOP);
	Button_SetImage(Dlg(IDBNEXT),IDBITMAPNEXT);
	Button_SetImage(Dlg(IDBCONNECT),IDBITMAPDISCONNECTED);
	Button_SetImage(Dlg(IDREPEAT),IDBITMAPREPEAT);
	Button_SetImage(Dlg(IDSHUFFLE),IDBITMAPSHUFFLE);

	ScrollBar_SetRange(Dlg(IDVOL),0,100,TRUE);
	nRet=WSAStartup(wVersionReq,&wsaData);
	if(nRet || (wsaData.wVersion!=wVersionReq)){
		PRNERROR("Winsock version error");
		WSACleanup();
		return 0;
	}

	memset(&nid,0,sizeof(NOTIFYICONDATA));
	nid.uID=999;
	nid.cbSize=sizeof(nid);
	nid.hWnd=hDlg;
	nid.hIcon=LoadIcon(hInstDlg,MAKEINTRESOURCE(IDAPPICON));
	nid.uCallbackMessage=0x401;
	nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
	nid.uVersion=3;
	Shell_NotifyIcon(NIM_ADD,&nid);

	Shell_NotifyIcon(NIM_SETVERSION,&nid);

	return 1;
}

static BOOL CALLBACK DialogFunc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam){
	char szbuf[1024];
	signed int param1;

	switch (msg) {
		case WM_INITDIALOG:
			InitializeApp(hwndDlg,wParam,lParam);
			Resize();
			return TRUE;
			break;
		case SOCKET_READY:
			nullStr(szbuf);
			recv(connection,szbuf,sizeof(szbuf),0);
			ParseCmd(szbuf);
			break;
		case WM_SIZING:{
			if(!IsIconic(hWndDlg)){
				LPRECT lprc=(LPRECT)lParam;
				if( lprc->right < 370)
					lprc->right = 370;
				if( lprc->bottom < 230)
					lprc->bottom = 230;
			}
			}break;
		case WM_SIZE:
			if(IsIconic(hWndDlg)){
				ShowWindow(hWndDlg,SW_HIDE);
				return 0;
			} else Resize();
			break;
		case 0x401: switch(lParam){
			case WM_LBUTTONUP:
				ShowWindow(hWndDlg, SW_SHOWNORMAL);
				break;
			case WM_RBUTTONUP:{
				POINT p; GetCursorPos(&p);
				HMENU pop=CreatePopupMenu();
				AppendMenu(pop,0,IDMEXIT,"Exit");
				TrackPopupMenu(pop,TPM_RIGHTALIGN,p.x,p.y,0,hWndDlg,NULL);
				}break;
			} break;
		case WM_COMMAND:
			nullStr(szbuf);
			switch (LOWORD(wParam)) {
				case IDBCONNECT:
					Status("Connecting...");
					if(connection==FALSE) Connect();
					else Disconnect();
					break;
				case IDBPREV:
					sprintf(szbuf,"PREV_TRACK\n");
					break;
				case IDBPLAY:
					sprintf(szbuf,"PLAY\n");
					break;
				case IDBPAUSE:
					sprintf(szbuf,"PAUSE\n");
					break;
				case IDBSTOP:
					sprintf(szbuf,"STOP\n");
					break;
				case IDBNEXT:
					sprintf(szbuf,"NEXT_TRACK\n");
					break;
				case IDREPEAT:
					if(Button_GetCheck(Dlg(IDREPEAT))==BST_CHECKED)
						sprintf(szbuf,"REPEAT OFF\n");
					else if(Button_GetCheck(Dlg(IDREPEAT))==BST_UNCHECKED)
						sprintf(szbuf,"REPEAT ON\n");
					break;
				case IDSHUFFLE:
					if(Button_GetCheck(Dlg(IDSHUFFLE))==BST_CHECKED)
						sprintf(szbuf,"SHUFFLE OFF\n");
					else if(Button_GetCheck(Dlg(IDSHUFFLE))==BST_UNCHECKED)
						sprintf(szbuf,"SHUFFLE ON\n");
					break;
				case IDLIST: switch(HIWORD(wParam)){
					case LBN_DBLCLK:
						param1=ListBox_GetItemData(Dlg(IDLIST),ListBox_GetCurSel(Dlg(IDLIST)));
						sprintf(szbuf,"JUMP_TO_TRACK %d\n",param1);
						Status(szbuf);
						ClearIDEDIT();
						break;
				} break;
				case IDBGO:
					param1=ListBox_GetItemData(Dlg(IDLIST),ListBox_GetCurSel(Dlg(IDLIST)));
					sprintf(szbuf,"JUMP_TO_TRACK %d\n",param1);
					Status(szbuf);
					ClearIDEDIT();
					break;
				case IDEDIT: switch(HIWORD(wParam)){
					case EN_CHANGE:
						FindSong();
						break;
				} break;
				case IDMEXIT:
					Disconnect();
					WSACleanup();
					Shell_NotifyIcon(NIM_DELETE,&nid);
					EndDialog(hwndDlg,0);
					return TRUE;
			}
			Send(szbuf);
			break;
		case WM_CLOSE:
			Disconnect();
			WSACleanup();
			Shell_NotifyIcon(NIM_DELETE,&nid);
			EndDialog(hwndDlg,0);
			return TRUE;
			break;
		case WM_HSCROLL:
			param1=ScrollBar_GetPos(Dlg(IDPOS));
			switch(LOWORD(wParam)){
				case SB_PAGELEFT:
					param1-=5;
					break;
				case SB_LINELEFT:
					param1-=2;
					break;
				case SB_LINERIGHT:
					param1+=2;
					break;
				case SB_PAGERIGHT:
					param1+=5;
					break;
				case SB_THUMBTRACK:
					param1=HIWORD(wParam);
					PosUpdate=FALSE;
					break;
				case SB_ENDSCROLL:
					PosUpdate=TRUE;
					break;
			}
			ScrollBar_SetPos(Dlg(IDPOS),param1,TRUE);
			if(PosUpdate){
				sprintf(szbuf,"JUMP_TO_TIME %d\n",param1);
				Send(szbuf);
			}

			break;
		case WM_VSCROLL:
			param1=ScrollBar_GetPos(Dlg(IDVOL));
			switch(LOWORD(wParam)){
				case SB_LINEDOWN:
					if(param1>0) param1-=5;
					break;
				case SB_LINEUP:
					if(param1<100) param1+=5;
					break;
				case SB_THUMBTRACK:
					param1=HIWORD(wParam);
					break;
			}
			nullStr(szbuf);
			sprintf(szbuf,"VOLUME %d\n",param1);
			Send(szbuf);
			ScrollBar_SetPos(Dlg(IDVOL),param1,TRUE);
			UpdateStatic(param1);
			break;
	}
	return FALSE;
}

SOCKET CallServer(LPCSTR lpServerName){
	IN_ADDR		iaHost;
	LPHOSTENT	lpHostEntry;

	iaHost.s_addr=inet_addr(lpServerName);
	if(iaHost.s_addr == INADDR_NONE){
		lpHostEntry=gethostbyname(lpServerName);
	} else {
		lpHostEntry=gethostbyaddr((const char *)&iaHost,
			sizeof(struct in_addr), AF_INET);
	}
	if(lpHostEntry==NULL){
		Status("Error looking up host");
		return INVALID_SOCKET;
	}

	SOCKET Socket;
	Socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(Socket==INVALID_SOCKET){
		Status("Error creating socket");
		return INVALID_SOCKET;
	}

	SOCKADDR_IN saServer;
	saServer.sin_port=htons(1586);
	saServer.sin_family=AF_INET;
	saServer.sin_addr=*((LPIN_ADDR)*lpHostEntry->h_addr_list);

	int nRet;
	nRet=connect(Socket,(LPSOCKADDR)&saServer,sizeof(SOCKADDR_IN));
	if(nRet==SOCKET_ERROR){
		closesocket(Socket);
		Status("Error connecting to port");
		return INVALID_SOCKET;
	}
	return Socket;

/*	sprintf(szBuffer,"PLAY\n");
//	AddItem(szBuffer);
//	nRet=send(Socket,szBuffer,strlen(szBuffer),0);
//	if(nRet==SOCKET_ERROR){
//		closesocket(Socket);
//		PRNERROR("send()");
//		return INVALID_SOCKET;
//	}
//	while(nRet>=0){
//	nRet = recv(Socket, szBuffer, sizeof(szBuffer), 0);
//	AddItem(szBuffer);
//	static char szBuffer[256];
//	memset(szBuffer,0,sizeof(szBuffer));
//	strcat(szBuffer,"PLAY\r\nEXIT\r\n");
//	nRet=send(Socket,szBuffer,strlen(szBuffer),0);
//	closesocket(Socket);
*/
}

void ParseCmd(LPCSTR str){
	int cmd;
	int param1,param2;
	HWND hItem;
	char szbuf[1024];
	nullStr(szbuf);
	sscanf(str,"%d %s",&cmd,szbuf);
	switch(cmd){
		case  0:	// Welcome
			Send("REQUEST PLAYLIST\n");
			break;
		case  1:	// Authentication / failure
			Status("Authentication failure");
			break;
		case  2:	// Unknown command
			Status("Unknown command");
			break;
		case  3: 	// Too few arguments
			Status("Too few arguments");
			break;
		case  4:	// Unrecognized/invalid argument
			Status("Invalid argument");
			break;
		case  5:	// Last request OK
//			Status("OK");
			break;
		case 10:{	// Player status "Playing vol Shuffle Repeat"
			int r,s;
			char szshuf[12];	nullStr(szshuf);
			char szrepeat[12];	nullStr(szrepeat);
			sscanf(str,"010 %s %d %s %s",szbuf,&param1,szshuf,szrepeat);
			ScrollBar_SetPos(Dlg(IDVOL),param1,TRUE);
			r=strcmp(szrepeat,"SINGLE")==0 ? BST_UNCHECKED : BST_CHECKED;
			s=strcmp(szshuf,"NORMAL\0\0")  ==0 ? BST_UNCHECKED : BST_CHECKED;
			Button_SetCheck(Dlg(IDREPEAT),r);
			Button_SetCheck(Dlg(IDSHUFFLE),s);
			UpdateStatic(param1);
			Status(szbuf);
			} break;
		case 11:{	// Current track info "song path len kb hz ch
			char status[256];
			int i;
			nullStr(status);
			sscanf(str ,"%d \"%[^\"]\"",&cmd, status);
			Status(status);
			Balloon(status,"playing");
			}break;
		case 12:{	// Current playlist info "# ##"
			sscanf(str,"012 %d %d",&param1,&param2);
			sprintf(szbuf,"Playlist [%u/%u]",param1,param2);
			Status(szbuf);
			ClearIDEDIT();
			ListBox_SetCurSel(Dlg(IDLIST),param1);
			if((param2) != ListBox_GetCount(Dlg(IDLIST))){
				ResetList();
				if((param2) != ListBox_GetCount(Dlg(IDLIST)))
					LoadPlaylist();
			}
			}break;
		case 13:	// Track playing update "hms hms"
			if(PosUpdate){
				int h,m,s,lh,lm,ls;
				sscanf(str,"013 %d:%d:%d %d:%d:%d",&h,&m,&s,&lh,&lm,&ls);
				m+=h*60; s+=m*60;
				lm+=lh*60; ls+=lm*60;
				ScrollBar_SetRange(Dlg(IDPOS),0,ls,FALSE);
				ScrollBar_SetPos(Dlg(IDPOS),s,TRUE);
			}
			break;
		case 14:
			Status("Reading playlist");
			break;
	}
}

void Resize(){
	RECT r;
	int SPACE=70;
	int TOP;
	int minWidth=300;
	int minHeight=300;
	GetClientRect(hWndDlg,&r);
	TOP=r.bottom-SPACE;
	MoveWindow(Dlg(IDEDIT),4,4,r.right-8,20,TRUE);
	MoveWindow(Dlg(IDLIST),4,25,r.right-8,r.bottom-SPACE-30,TRUE);
	MoveWindow(Dlg(IDFIND),4,25,r.right-8,r.bottom-SPACE-30,TRUE);
	MoveWin(Dlg(IDBPREV),	10,	TOP);
	MoveWin(Dlg(IDBPLAY),	40,	TOP);
	MoveWin(Dlg(IDBPAUSE),	70,	TOP);
	MoveWin(Dlg(IDBSTOP),	100,TOP);
	MoveWin(Dlg(IDBNEXT),	130,TOP);
	MoveWin(Dlg(IDVOL)	,	169,TOP-2);
	MoveWin(Dlg(IDPBORDER),	160,TOP-2);
	MoveWin(Dlg(IDBCONNECT),r.right-45,TOP);
	MoveWin(Dlg(IDSHUFFLE), 190,TOP);
	MoveWin(Dlg(IDREPEAT),	215,TOP);
	MoveWindow(Dlg(IDSTATUS),2,r.bottom-18,r.right-4,18,TRUE);
	MoveWindow(Dlg(IDPOS),12,TOP+30,r.right-24,14,TRUE);
	UpdateStatic(ScrollBar_GetPos(Dlg(IDVOL)));
/*
//	RECT wpos;
//	GetWindowRect(hWndDlg,&wpos);
//	if(r.right<minWidth)
//		SetWindowPos(hWndDlg,NULL,0,0,minWidth,wpos.bottom-wpos.top,SWP_NOMOVE|SWP_NOZORDER);
//	if(r.bottom<minHeight)
//		SetWindowPos(hWndDlg,NULL,0,0,wpos.right-wpos.left,minHeight,SWP_NOMOVE|SWP_NOZORDER);
//	SetMinMaxInfo();
*/
}
void MoveWin(HWND h, int x, int y){
	RECT r;
	GetClientRect(h,&r);
//	MoveWindow(h,x,y,r.right,r.bottom,TRUE);
	SetWindowPos(h,NULL,x,y,0,0,SWP_NOSIZE);
}

void UpdateStatic(int vol){
	RECT r,w;
	int hgt;
	int x,y;
	GetWindowRect(Dlg(IDPBORDER),&r);
	ScreenToClient(hWndDlg,&r);
	x=r.left;
	y=r.top;
	GetClientRect(Dlg(IDPBORDER),&r);
	hgt=(r.bottom*vol/100)-2;
	MoveWindow(Dlg(IDPFILL),x+1,y+(r.bottom-hgt-1),r.right-2,hgt,TRUE);
}

void LoadPlaylist(){
	char szbuf[8192];
	char data[131000];
	char status[32];
	unsigned long sz,szprev;
	unsigned int len,i;
	int tOut=10;
	SYSTEMTIME time,ctime;

	OffSignal();
	Status("Reading playlist...");

	nullStr(data);
	nullStr(status);
	Cursor(IDC_WAIT);
	i=0;
	szprev=0;
	int ctout;
	ctout=0;

	Send("REQUEST PLAYLIST_TRACKS\n");
	while(1){
		ioctlsocket(connection,FIONREAD,&sz);
		if(sz>0){
			nullStr(szbuf);
			recv(connection,szbuf,sizeof(szbuf),0);
			strcat(data,szbuf);
			sprintf(status,"Reading bytes: %d",strlen(data));
			i++;
			Status(status);
			GetSystemTime(&time);
		}
		if(strstr(data,"\n005 ")!=NULL) break;
		GetSystemTime(&ctime);
		if((ctime.wSecond - time.wSecond) %tOut==(tOut-1)) {
			Status("Timed out reading playlist");
			Disconnect();
			return;
		}
		if((ctime.wSecond-time.wSecond)!=ctout) {
			ctout = ctime.wSecond - time.wSecond;
			if(ctout>0)
				sprintf(status,"Reading bytes: %d, timeout %u",strlen(data),ctout);

			Status(status);
		}

	}

	char *tok;
	char t1[128],t2[128],t3[128];
	nullStr(t1); nullStr(t2); nullStr(t3);
	i=1;
	LLIST *lTemp=lPlaylist.next;

	LLIST *tmp=lTemp;
	Status("Clearing list");
	while(tmp){
		tmp=lTemp->next;
		lTemp->next=NULL;
		free(lTemp);
		lTemp=tmp;
	} //free(&lPlaylist);

	lTemp=&lPlaylist;

	for(tok=strtok(data,"\n"); tok!=NULL; tok=strtok(NULL,"\n")){
		int n;
		nullStr(t1); nullStr(t2); nullStr(t3);
		sscanf(tok,"014 %s \"%[^\"]\" \"%[^\"]\"",t1,t2,t3);
		if(t2[0]!=0){
			LLIST *l;
			l=malloc(sizeof *l);
			sprintf(l->data,"%s",t2);
			l->next=NULL;
			lTemp->next=l;
			lTemp=lTemp->next;
		}
		i++;
	}

	ResetList();
	Cursor(IDC_ARROW);
	OnSignal();
	Send("REQUEST PLAYLIST\n");
}

void Connect(){
	Cursor(IDC_WAIT);
	connection=CallServer("whiteguy.ath.cx");
	if(connection==INVALID_SOCKET){
		Status("Error looking up host");
		connection=FALSE;
		return;
	}
	Status("Connected");
	Button_SetImage(Dlg(IDBCONNECT),IDBITMAPCONNECTED);


	OnSignal();
	Cursor(IDC_ARROW);
}

void Disconnect(){
	Status("Hanging up...");
	Send("EXIT\n");
	closesocket(connection);
	connection=FALSE;
	Status("Disconnected");
	Button_SetImage(Dlg(IDBCONNECT),IDBITMAPDISCONNECTED);
	ListBox_ResetContent(Dlg(IDLIST));
}

void FindSong(){
	char szbuf[128];
	LLIST *lTemp=lPlaylist.next;
	Status("Searching...");
	if(Edit_GetTextLength(Dlg(IDEDIT))>0){
		int i=0;
		ListBox_ResetContent(Dlg(IDLIST));
		nullStr(szbuf);
		Edit_GetText(Dlg(IDEDIT),szbuf,Edit_GetTextLength(Dlg(IDEDIT))+1);
		Status(szbuf);
		while(lTemp){
			char szmatch[128];
			nullStr(szmatch);
			strcat(szmatch,szbuf);
			if( Matches(lTemp->data,szmatch)){
				AddItem(lTemp->data);
				ListBox_SetItemData(Dlg(IDLIST),ListBox_GetCount(Dlg(IDLIST))-1,i);
			}
			i++;
			lTemp=lTemp->next;
		}
		ListBox_SetCurSel(Dlg(IDLIST),0);

	} else{
		ResetList();
	}
	Status("Ready.");
}

BOOL Matches(char *song,char *szdata){
	char *tok;
	char *words[20];
	char status[64];
	int i=0;
	for(tok=strtok(szdata," "); tok!=NULL; tok=strtok(NULL," ")){
		words[i++]=tok;
	}
	int l=i;
	for(int i=0; i<l; i++){
		if(!stristr(song,words[i]))
			return FALSE;
	}
	return TRUE;
}

void ResetList(){
	LLIST *lTemp=lPlaylist.next;
	Status("Listing...");
	ListBox_ResetContent(Dlg(IDLIST));
	int i=0;
	while(lTemp){
		AddItem(lTemp->data);
		ListBox_SetItemData(Dlg(IDLIST),ListBox_GetCount(Dlg(IDLIST))-1,i++);
		lTemp=lTemp->next;
	}
	Status("Ready.");
}

const char *String(char *format,...){
	const char *szbuf;
	va_list theParms;
	va_start(theParms,format);
	szbuf=malloc(256);
	sprintf(szbuf,format,theParms);
	va_end(theParms);
	return szbuf;
}

void Balloon(char *Title, char *Message){
//	nid.cbSize=504;
	nid.uFlags=NIF_INFO;
	nid.hWnd=hWndDlg;
	strcpy(nid.szInfoTitle,Title);
	strcpy(nid.szInfo,Message);
	nid.dwInfoFlags=NIIF_INFO | NIIF_NOSOUND;
	nid.uVersion=1000;
	Shell_NotifyIcon(NIM_MODIFY,&nid);
}
