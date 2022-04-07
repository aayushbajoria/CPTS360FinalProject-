int make_dir(char *pathname)
{
    int ino, r, dev;
	MINODE *pip;
	char *parent, *child;
	
	if (pathname[0] == 0)
	{
		printf("No pathname for a new directory given!\n");
		return 1;
	}

	// absolute vs. relative path checking
	if(pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	// find the in-memory minode of the parent
	if(findparent(pathname)) // root inode
	{  
		parent = dirname(pathname);
		child = basename(pathname);
		ino = getino(&dev, parent);

		if(!ino)
			return 1;
		pip = iget(dev, ino);
	}
	else // other inode
	{
		pip = iget(running->cwd->dev,running->cwd->ino);
		child = (char *)malloc((strlen(pathname) + 1) * sizeof(char));
		strcpy(child, pathname);
	}

	// verify INODE is a DIR
	if((pip->INODE.i_mode & 0040000) != 0040000) 
	{
		printf("%s is not a directory.\n", parent);
		iput(pip);
		return 1;
	}
	// verify child does not exist in the parent directory
	if(search(pip,child))
	{
		printf("%s already exists.\n", child);
		iput(pip);
		return 1;
	}

	// create the child
	r = my_mkdir(pip, child);

	return r;
}

int my_mkdir(MINODE *pip, char *name)
{
	unsigned long inumber, bnumber;
	int i, datab, need_length, ideal_length, rec_length;
	char buf[BLOCK_SIZE], buf2[BLOCK_SIZE], *cp;
	DIR *dirp;
	MINODE *mip;

	// allocate an inode and a disk block for the new directory
	inumber = ialloc(pip->dev);
	bnumber = balloc(pip->dev);

	mip = iget(pip->dev, inumber); // load the inode into a minode so we can write it to the disk

	// setup information for the new directory
	mip->INODE.i_mode = 0x41ED; // DIR and permissions
	mip->INODE.i_uid = running->uid; // owner uid
	mip->INODE.i_gid = running->gid; // group id
	mip->INODE.i_size = 1024; // size in bytes
	mip->INODE.i_links_count = 2; // links count
	mip->INODE.i_atime = mip->INODE.i_ctime = mip->INODE.i_mtime = time(0L); // creation time
	mip->INODE.i_blocks = 2; // blocks count in 512-byte blocks
	mip->dirty = 1; // mark as dirty

	for(i = 1; i < 15; i++) // clear the data blocks
		mip->INODE.i_block[i] = 0;
	mip->INODE.i_block[0] = bnumber; // set block 0 to the allocated bnumber

	iput(mip); // cleanup

	// write the . and .. entries into a buf[] of BLOCK_SIZE
	memset(buf, 0, BLOCK_SIZE);
	dp = (DIR *)buf;
	
	dp->inode = inumber; // inode number
	strncpy(dp->name, ".", 1); // file name
	dp->name_len = 1; // name length
	dp->rec_len = 12; // directory entry length

	cp = buf + dp->rec_len;
	dp = (DIR *)cp;

	dp->inode = pip->ino; // inode number of parent DIR
	dp->name_len = 2; // name length
	strncpy(dp->name, "..", 2); // file name
	dp->rec_len = BLOCK_SIZE - 12; // last DIR entry length to end of block

	put_block(pip->dev, bnumber, buf); // save block to disk

	// Finally enter name into parent's directory...
	// get iblock count
	i = 0;
	while(pip->INODE.i_block[i])
		i++;
	i--;

	get_block(pip->dev, pip->INODE.i_block[i], buf);
	dp = (DIR *)buf;
	cp = buf;
	rec_length = 0;

	// step to the last entry in a data block
	while(dp->rec_len + rec_length < BLOCK_SIZE)
	{
		rec_length += dp->rec_len;
		cp += dp->rec_len;
		dp = (DIR *)cp;
	}

	// when entering a new entry with name_len = n
	need_length = 4 * ((8 + strlen(name) + 3) / 4); // a multiple of 4
	// step to the last entry in a data block...its ideal length is...
	ideal_length = 4 *((8 + dp->name_len + 3) / 4);
	rec_length = dp->rec_len; // store rec_len for a bit easier code writing

	// check if it can enter the new entry as the last entry
	if((rec_length - ideal_length) >= need_length)
	{
		// trim the previous entry to its ideal_length
		dp->rec_len = ideal_length;
		cp+=dp->rec_len;
		dp = (DIR *)cp;
		dp->rec_len = rec_length - ideal_length;
		dp->name_len = strlen(name);
		strncpy(dp->name, name, dp->name_len);
		dp->inode = inumber;

		// write the new block back to the disk
		put_block(pip->dev, pip->INODE.i_block[i], buf);
	}
	else
	{
		// otherwise allocate a new data block 
		i++;
		datab = balloc(pip->dev);
		pip->INODE.i_block[i] = datab;
		get_block(pip->dev, datab, buf2);

		// enter the new entry as the first entry in the new block
		dirp = (DIR *)buf2;
		dirp->rec_len = BLOCK_SIZE;
		dirp->name_len = strlen(name);
		strncpy(dirp->name, name, dirp->name_len);
		dirp->inode = inumber;

		pip->INODE.i_size += BLOCK_SIZE;
		
		// write the new block back to the disk
		put_block(pip->dev, pip->INODE.i_block[i], buf2);
	}

	// inc parent inode's link count by 1, touch its atime, and mark it dirty
	pip->INODE.i_links_count++;
	pip->INODE.i_atime = time(0L);
	pip->dirty = 1;
	iput(pip); // cleanup
	return 0;
}
