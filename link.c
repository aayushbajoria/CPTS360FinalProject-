int Link()
{
	char parent[256], child[256], buf[BLOCK_SIZE], buf2[BLOCK_SIZE], *cp;
	unsigned long inumber, oldIno;
	int dev, newDev, i, rec_length, need_length, ideal_length, datab;
	MINODE *mip;
	DIR *dirp;
	
	if (pathname[0] == 0)
	{
		printf("No pathname for the original file given!\n");
		return 1;
	}
	if (parameter[0] == 0)
	{
		printf("No pathname for the new hardlink file given!\n");
		return 1;
	}

	// absolute vs. relative paths
	if(pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	// get the inumber of the original file, if it exists
	oldIno = getino(&dev, pathname);
	if(!oldIno)
		return 1;

	mip = iget(dev,oldIno);

	// verify the original file is a REG file...DIRs are not invited :(
	if(((mip->INODE.i_mode) & 0100000) != 0100000)  
	{
		printf("%s is not REG file.\n",pathname);
		iput(mip);
		return 1;
	}

	iput(mip);

	// absolute vs. relative stuffs for new file
	if(parameter[0] == '/')
		newDev = root->dev;
	else
		newDev = running->cwd->dev;

	// find the parent inode for the new file
	if(findparent(parameter))
	{
		strcpy(parent, dirname(parameter));
		strcpy(child, basename(parameter));

		inumber = getino(&newDev, parent);

		if(!inumber)
			return 1;

		if(newDev != dev)
		{
			printf("How did this even happen?!?!??!?!?!?!\n");
			return 1;
		}

		mip = iget(newDev,inumber);

		// verify parent is a DIR
		if(((mip->INODE.i_mode) & 0040000) != 0040000)  
		{
			printf("%s is not DIR file.\n", parent);
			iput(mip);
			return 1;
		}

		// verify child does not exist in the parent DIR
		if(search(mip, child))
		{
			printf("%s already exists!\n", child);
			iput(mip);
			return 1;
		}
	}
	else
	{
		strcpy(parent, ".");
		strcpy(child, parameter);

		if(running->cwd->dev != dev)
		{
			printf("It is amazing that this managed to show up!\n");
			return -1;
		}

		// verify the file does not already exist
		if(search(running->cwd, child))
		{
			printf("%s already exists.\n", child);
			return -1;
		}
		mip = iget(running->cwd->dev, running->cwd->ino);
	}     

	// add an entry to the parent's data block
	i = 0;
	while(mip->INODE.i_block[i])
		i++;
	i--;  

	get_block(mip->dev, mip->INODE.i_block[i], buf);
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

	need_length = 4 * ((8 + strlen(child) + 3) / 4);
	ideal_length = 4 *((8 + dp->name_len + 3) / 4);
	rec_length = dp->rec_len;
	
	// check if it can enter the new entry as the last entry
	if((rec_length - ideal_length) >= need_length)
	{
		// trim the previous entry to its ideal_length
		dp->rec_len = ideal_length;
		cp += dp->rec_len;
		dp = (DIR *)cp;
		dp->rec_len = rec_length - ideal_length;
		dp->name_len = strlen(child);
		strncpy(dp->name, child, dp->name_len);
		dp->inode = oldIno;

		// write the new block back to the disk
		put_block(mip->dev, mip->INODE.i_block[i], buf);
	}
	else
	{
		// otherwise allocate a new data block 
		i++;
		datab = balloc(mip->dev);
		mip->INODE.i_block[i] = datab;
		get_block(mip->dev, datab, buf2);

		// enter the new entry as the first entry 
		dirp = (DIR *)buf2;
		dirp->rec_len = BLOCK_SIZE;
		dirp->name_len = strlen(child);
		strncpy(dirp->name, child, dirp->name_len);
		dirp->inode = oldIno;
		mip->INODE.i_size += BLOCK_SIZE;
		// write the new block back to the disk
		put_block(mip->dev, mip->INODE.i_block[i], buf2);
	}

	mip->INODE.i_atime = time(0L);
	mip->dirty = 1;
	iput(mip);
	mip= iget(newDev, oldIno);
	mip->INODE.i_links_count++; // increment the i_links_count of INODE by 1
	mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
	mip->dirty = 1;
	iput(mip); // write it back to the disk

	return 0;
}


