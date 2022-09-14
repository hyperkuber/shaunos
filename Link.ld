
ENTRY(start)
SECTIONS
{
  . = __PAGE_OFFSET + 0x100000;
   _text = .;
  .text : {
    code = .; _code = .; __code = .; 
    *(.text)
    . = ALIGN(4096);
  }
  _etext = .;
  .data :{
     data = .; _data = .; __data = .;
     *(.data)
     *(.rodata)
     . = ALIGN(4096);
  }
  _edata = .;
  __bss_start = .;	/* BSS */
  .bss : {
    bss = .; _bss = .; __bss = .;
    . = ALIGN(4096);
	*(.bss.page_aligned)
	*(.bss)
	*(COMMON)
  }
  . = ALIGN(4);
  __bss_stop = .; 

  end = .; _end = .; __end = .;
  . = ALIGN(4096);
  pg0 = .;
}
