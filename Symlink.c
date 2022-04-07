int CMD_SYMLINK()
{
	unsigned long inumber;
	int dev, i, r;
	MINODE *mip, *pip;
	char *cp, *parent, *child, buf[BLOCK_SIZE];
	DIR *dirp;
	
	if (pathname[0] == 0)
	{
		printf("No pathname for the original file given!\n");
		return 1;
	}
	if (parameter[0] == 0)
	{
		printf("No pathname for the new symlink file given!\n");
		return 1;
	}

	// absolute vs. relative path stuffs
	if(pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	// check if old name exists
	inumber = getino(&dev, pathname);
	if(!inumber)
		return 1;

	// get the minode for the old name
	mip = iget(dev, inumber);

	// make sure it is REG file
	if(((mip->INODE.i_mode) & 0100000) != 0100000 && (((mip->INODE.i_mode) & 0040000) != 0040000))  
	{
		printf("%s is not a REG file or DIR.\n",pathname);
		iput(mip);
		return -1;
	}
	iput(mip);

	// absolute vs. relative path stuff
	if(parameter[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	// find the parent inode for the new file
	if(findparent(parameter))
	{  
		parent = dirname(parameter);
		child = basename(parameter);
		inumber = getino(&dev, parent);

		if(!inumber)
			return 1;

		pip = iget(dev, inumber);
	}
	else
	{
		pip = iget(running->cwd->dev,running->cwd->ino);
		child = (char *)malloc((strlen(parameter) + 1) * sizeof(char));
		strcpy(child, parameter);
		parent = (char *)malloc(2 * sizeof(char));
		strcpy(parent,".");
	}

	// verify that the parent is, in fact, a DIR
	if((pip->INODE.i_mode & 0040000) != 0040000) 
	{
		printf("%s is not a directory.\n", parent);
		iput(pip);
		return 1;
	}

	// verify that the new file doesn't already exist
	if(search(pip, child))
	{
		printf("%s already exists.\n", child);
		iput(pip);
		return 1;
	}

	r = my_creat(pip, child);

	// get the new path into memory
	inumber = getino(&dev, parameter);

	if(!inumber)
		return 1;

	mip = iget(dev, inumber);
	mip->INODE.i_mode = 0xA1FF; // change the mode of the new file to a symlink
	strcpy((char *)(mip->INODE.i_block), pathname);
	mip->INODE.i_size = strlen(pathname);
	mip->dirty = 1;
	iput(mip);
	
	return r;
}
