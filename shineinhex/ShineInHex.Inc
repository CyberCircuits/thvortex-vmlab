include /masm32/include/windows.inc
include /masm32/include/user32.inc
include /masm32/include/kernel32.inc
include /masm32/include/gdi32.inc
include /masm32/include/comctl32.inc
include /masm32/include/msimg32.inc
include /masm32/include/masm32.inc

includelib /masm32/lib/user32.lib
includelib /masm32/lib/kernel32.lib
includelib /masm32/lib/gdi32.lib
includelib /masm32/lib/comctl32.lib
includelib /masm32/lib/msimg32.lib
includelib /masm32/lib/masm32.lib

CallProc		PROTO :DWORD,:DWORD,:DWORD,:DWORD
CreateTextTable	PROTO
AddRecentBytes  PROTO :DWORD
CreateClass     PROTO
to_UTF8   PROTO :DWORD,:DWORD
DlgFind   PROTO :DWORD,:DWORD,:DWORD,:DWORD
DlgSelect PROTO :DWORD,:DWORD,:DWORD,:DWORD
Draww PROTO
InvalidateSel PROTO :DWORD
SetScreenColors PROTO
DrawBase   PROTO
DrawHeader PROTO
DrawOffset PROTO
ClearUndoBuf PROTO
AllocUndo    PROTO
my_slow_cmp  PROTO :DWORD,:DWORD,:DWORD
DoUndo       PROTO
DoRedo       PROTO
SendNotify   PROTO
SendChangeNotify PROTO :DWORD,:DWORD,:DWORD
PositionCaret  PROTO
;-------


m2m	MACRO mem32A, mem32B
	mov	 eax, mem32B
	mov	 mem32A, eax
ENDM

WndProcThunk STRUCT BYTE
	m_mov     dd ?
	m_this    dd ?
	m_jmp     db ?
	m_relproc dd ?
WndProcThunk ENDS

SIZEX STRUCT
	cx_ dd ?
	cy_ dd ?
SIZEX ENDS

UNDO STRUCT
	un_pointer dd ? ; globalloc
	un_size    dd ?
	un_offset  dd ?
	un_next    dd ? ; globalloc
	un_prev    dd ?
UNDO ENDS

HEXNM_SHOWMENU struct
	hdr						NMHDR <?> ; standard WM_NOTIFY header
	hex_popup				dd ? ; copy of HM.hex_popup
HEXNM_SHOWMENU ends

HEXNM_CUSTOMCOLORS struct
	hdr						NMHDR <?> ; standard WM_NOTIFY header
	colors_hex				dd ? ; copy of HM.screen_colors_st2
	colors_ansi				dd ? ; copy of HM.screen_colors_st3
	sc_start				dd ? ; (sc_start + HM._start) from SetScreenColors
	sc_end					dd ? ; (sc_end + HM._start) from SetScreenColors
HEXNM_CUSTOMCOLORS ends

HEXNM_CHANGE struct
	hdr						NMHDR <?> ; standard WM_NOTIFY header
	_start					dd ? ; (HM.sel_start + HM._start)
	_end					dd ? ; (HM.sel_end + HM._start)
HEXNM_CHANGE ends

HM struct
	hWnd       				dd ? ; handle of control
	screen_colors_st2		dd ? ; Pointer to color buffer
	screen_colors_st3 		dd ? ; Pointer to color buffer
	screen_colors_len		dd ? ; Size (in displayed chars) of color buffers (screen_colors_len * 8 + 8 = byte size of each buffer)
	sc_start				dd ? ; offset (not an address) of first visible byte on screen
	sc_end					dd ? ; offset (not an address) of last visible byte on screen
	bk_color				dd 12 dup (?)
	text_color				dd 12 dup (?)
	rc1 					RECT <?> ; 1st stage rect
	rc2 					RECT <?> ; 2nd stage rect
	rc3 					RECT <?> ; 3rd stage rect
	sel_rc 					RECT <?> ; 2-3 stage rect
	main_rc   				RECT <?> ; client area rect
	hex_popup      	   		dd ? ; main popup
	hex_popupA              dd ? ; popup for Copy->
	update_st				dd ?
	view_lenx  				dd ? ; number of characters that fits horizontally (per stage)
	view_leny 				dd ? ; number of characters that fits vertically (excluding header)
	vscrollpos 				dd ? ; current sinfo.nPos
	byte_nr    				dd ?
	_size      				dd ? ; size of data
	_start     				dd ? ; offset
	_nMax      				dd ? ; number of lines
	font       				dd ? ; handle of currently selected font
	fon_cx     				dd ? ; aver. width of one character
	fon_cy     				dd ? ; aver. hight of one character
	sel_start  				dd ? ; selection start offset (not an address)
	sel_end    				dd ? ; selection end   offset (not an address)
	sel_last                dd ?
	prev_sel_start			dd ? ; previous sel_start when HEXN_SELCHANGED was sent
	prev_sel_end			dd ? ; previous sel_end when HEXN_SELCHANGED was sent
	pointer    				dd ? ; pointer of data
	timer_direction 		dd ? ; 1 - up, 2 -down (scroll direction)
	input_mode              dd ? ; 0 - from 2nd stage, 1 - from 3rd stage
	modified_bytes  		dd 24 dup (?)
	modified_counter        dd ? ; how many bytes are modified (stops counting on 24)
	shift                   dd ? ; is shift pressed
	ctrl                    dd ? ; is ctrl  pressed
	mdc						dd ?
	counter					dd ?
	can_undo				dd ?
	can_redo				dd ?
	is_readonly             dd ?
	lf_choose				dd 5 dup (?)
	undo_start				dd ?
	find_hwnd				dd ? ; handle of "Find" dialog (if open)
	select_hwnd				dd ? ; handle of "Select" dialog (if open)
	end_of_line				dd ? ; is cursor at the end of line because of VK_END
	caret_posx				dd ? ; blinking caret horizontal position (in window client area pixels)
	caret_posy				dd ? ; blinking caret vertical position (in window client area pixels)
	caret_visible			dd ? ; is blinking caret within visible portion of window
	focus					dd ? ; does hex editor have keyboard focus
	union nm
		hdr					NMHDR <> ; standard WM_NOTIFY header
		menu				HEXNM_SHOWMENU <> ; used with HEXN_SHOWMENU notify
		colors				HEXNM_CUSTOMCOLORS <> ; used with HEXN_CUSTOMCOLORS notify
		change				HEXNM_CHANGE <> ; used with HEXN_SELCHANGE and HEXN_DATACHANGE notify
	ends
HM ends

.const
	HEXM_SETFONT          equ WM_USER+100 ;{wParam-font_number: 0 - Fixedsys, 1 - Courier, 2 - Terminal, 3 - Courier New, 4 - Lucida Console} {lParam-0}
	HEXM_SETOFFSETGRADCOL equ WM_USER+101 ;{wParam-grad_start_color}{lParam-grad_end_color	 }
	HEXM_SETHEADERGRADCOL equ WM_USER+102 ;{wParam-grad_start_color}{lParam-grad_end_color	 }
	HEXM_SETVIEW1TEXTCOL  equ WM_USER+103 ;{wParam-info_text_color }{lParam-view1_text_color }
	HEXM_SETHEADERTEXTCOL equ WM_USER+104 ;{wParam-addr_text_color }{lParam-switch_text_color}
	HEXM_SETVIEW2COL      equ WM_USER+105 ;{wParam-bk_color}     	{lParam-text_color}
	HEXM_SETVIEW3COL      equ WM_USER+106 ;{wParam-bk_color}     	{lParam-text_color}
	HEXM_SETVIEW2SELCOL   equ WM_USER+107 ;{wParam-bk_color}     	{lParam-text_color}
	HEXM_SETVIEW3SELCOL   equ WM_USER+108 ;{wParam-bk_color}     	{lParam-text_color}
	HEXM_SETACTIVECHARCOL equ WM_USER+109 ;{wParam-bk_color}     	{lParam-text_color}
	HEXM_SETMODBYTES1COL  equ WM_USER+110 ;{wParam-bk_color}     	{lParam-text_color}
	HEXM_SETMODBYTES2COL  equ WM_USER+111 ;{wParam-bk_color}     	{lParam-text_color}
	HEXM_SETMODBYTES3COL  equ WM_USER+112 ;{wParam-bk_color}     	{lParam-text_color}
	HEXM_SETPOINTER       equ WM_USER+113 ;{wParam-data pointer} 	{lParam-data size }
	HEXM_UNSETPOINTER     equ WM_USER+114 ;{wParam-0}            	{lParam-0         }
	HEXM_SETOFFSET        equ WM_USER+115 ;{wParam-offset}       	{lParam-0         }
	HEXM_SETSEL           equ WM_USER+116 ;{wParam-start_offs}		{lParam-end_offset}
	HEXM_UNDO             equ WM_USER+117 ;{wParam-0}	        	{lParam-0}
	HEXM_REDO             equ WM_USER+118 ;{wParam-0}	        	{lParam-0}
	HEXM_CANUNDO		  equ WM_USER+119 ;{wParam-0}	        	{lParam-0}
	HEXM_CANREDO          equ WM_USER+120 ;{wParam-0}	        	{lParam-0}
	HEXM_SETREADONLY      equ WM_USER+121 ;{wParam-is_readonly: 1,0}{lParam-0}
	HEXM_INVALIDATEALL    equ WM_USER+122 ;{wParam-0}           	{lParam-0}
	HEXM_GETHMENU         equ WM_USER+123 ;{wParam-0}	        	{lParam-0}
	HEXM_GETSEL           equ WM_USER+124 ;{wParam-ADDR start_offs}	{lParam-ADDR end_offset}
	HEXM_ALLOCUNDOREDO    equ WM_USER+125 ;{wParam-0}	        	{lParam-0}

	; Notification codes (NMHDR.code) used with WM_NOTIFY sent to parent window
	HEXN_SHOWMENU         equ 100    ;About to display popup menu; parent can customize menu
	HEXN_CUSTOMCOLORS     equ 101    ;Recomputing color tables; parent can customize display colors
	HEXN_SELCHANGE        equ 102    ;Current cursor position or selection range has changed
	HEXN_DATACHANGE       equ 103    ;Bytes in data buffer at cursor position or inside selection have changed
.data
	ALIGN 4
	measure_x db "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",0
	classname db "SHINEINHEX",0
	lf1       LOGFONT <12,8,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,0,"Fixedsys">
	lf2       LOGFONT <14,8,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,0,"Courier" >
	lf3       LOGFONT <12,8,0,0,400,0,0,0,OEM_CHARSET,    0,0,0,0,"Terminal">
	lf4       LOGFONT <16,8,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,0,"Courier New">
	lf5       LOGFONT <13,8,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,0,"Lucida Console">
	;lf_choose dd lf1,lf2,lf3,lf4,lf5
	m1 db "HEX ",0
	m2 db "ANSI",0
	m3 db "db %03xh,",0
	m4 db "%03xh,",0
	
	m5 db 34,"\x%02x",0
	m6 db "\x%02x",0
	m7 db "EOL", 0 ; TODO: DELETE ME ONCE BLINKING CARET SUPPORT IS DONE
	;--------------------------
	menui_undo	    db "Undo",0
	menui_redo      db "Redo",0
	
	menu_copy       db "Copy",0
	
	menui_copy_text db "Ansi",0
	menui_copy_hex  db "Hex",0
	menui_copy_db1  db "db sel",0
	menui_copy_db2  db "db �sel",0
	menui_copy_x01  db "\x00 sel",0
	menui_copy_x02  db "\x00 �sel",0
	
	menui_find      db "Find...",0
	menui_select    db "Select...",0
	
	error_hex      db "Need one more number",0
	hex_byte       db 0
	ALIGN 4
	hex_set1       dd 1
	hex_set2       dd 1
	hex_clip_copy  dd 0
	hex_clip_point dd 0
	ALIGN 4
.data?
	ALIGN 4
	found_offs 			dd ?
	checked    			dd ?
	glob_heap			dd ?
	hInstance			dd ?
	hex_nl				dd 3 	  dup (?)
	ALIGN 4
	one_line2			db 64      dup (?)
	one_line1			db 64*3    dup (?)
	hexlook				db 16*16*4 dup (?)
	ALIGN 4