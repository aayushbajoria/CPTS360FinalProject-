#include "functions.h"



/************* cd_ls_pwd.c file **************/
int cd()
{
 int ino = getino(pathname);
  if (ino == 0)
  {
    printf("Error: failed to traverse the path");
    return 0;
  }
  MINODE *mip = iget(dev, ino);

  // check if mip->INODE is a DIR
  if (!S_ISDIR(mip->INODE.i_mode))
  {
    printf("%s is not a directory\n", pathname);
    return 0;
  }

  iput(running->cwd); // release old cwd
  running->cwd = mip; // change cwd to mip
  return 0;
}

int ls_file(MINODE *mip, char *name)
{
   if (S_ISDIR(mip->INODE.i_mode))
    printf("d");
  else if (S_ISREG(mip->INODE.i_mode))
    printf("-");
  else if (S_ISLNK(mip->INODE.i_mode))
    printf("l");
  else
    printf("-");

  // print permission
  for (int i = 8; i >= 0; i--)
  {
    char c;
    if (i % 3 == 2)
    {
      if (mip->INODE.i_mode && (1 << i))
        c = 'r';
      else
        c = '-';
    }
    else if (i % 3 == 1)
    {
      if (mip->INODE.i_mode && (1 << i))
        c = 'w';
      else
        c = '-';
    }
    else
    {
      if (mip->INODE.i_mode && (1 << i))
        c = 'x';
      else
        c = '-';
    }
    putchar(c);
}

int ls_dir(MINODE *mip)
{
  printf("ls_dir: list CWD's file names; YOU FINISH IT as ls -l\n");

  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;

  get_block(dev, mip->INODE.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;
  
  while (cp < buf + BLKSIZE){
     strncpy(temp, dp->name, dp->name_len);
     temp[dp->name_len] = 0;
	
     printf("%s  ", temp);

     cp += dp->rec_len;
     dp = (DIR *)cp;
  }
  printf("\n");

  return 0;
}


int ls(char *pathname)
{
  MINODE *mip;
  int ino;
  // ls for cwd
  if (pathname[0] == 0)
  {
    ino = running->cwd->ino;
    mip = iget(dev, ino);
  }
  // ls with pathname
  else
  {
    // if path starts from root
    if (pathname[0] == '/')
      dev = root->dev;
    ino = getino(pathname);
    if (ino == 0)
    {
      printf("Error: Can not find pathname\n");
      return 0;
    }
    mip = iget(dev, ino);
    if (!S_ISDIR(mip->INODE.i_mode))
    {
      printf("Cannot traverse file that is not a directory\n");
      return 0;
    }
  }
  // printf("ls: list CWD only! YOU FINISH IT for ls pathname\n");
  ls_dir(mip);
}

void *pwd(MINODE *wd)
{
  // printf("pwd: READ HOW TO pwd in textbook!!!!\n");
  if (wd == root)
  {
    printf("/\n");
  }
  else{
    rpwd(wd);
  }
}

void rpwd(MINODE *wd)
{
  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;
  int parent_ino, my_ino;
  MINODE* pip;
  // get my_ino
  my_ino = wd->ino;
  // base case
  if (wd == root) return;

  get_block(dev, wd->INODE.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;

  // get parent_ino
  parent_ino = findino(wd, my_ino);
  pip = iget(dev, parent_ino);
  findmyname(pip, my_ino, buf);
  rpwd(pip);
  // release pip inode
  iput(pip);
  printf("/%s", buf);
}


