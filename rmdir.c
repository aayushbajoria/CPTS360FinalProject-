int CMD_RMDIR()
{
	int dev, i;
	unsigned long parent, ino;
	char *my_name;
	MINODE *mip, *pip;
	
	if (pathname[0] == 0)
	{
		printf("No pathname for a directory to remove given!\n");
		return 1;
	}

	// absolute vs. relative path stuffs
	if(pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	// get the inumber of the path
	ino = getino(&dev, pathname);

	// path doesn't exist
	if(!ino)
		return 1;

	// get a pointer to its minode[]
	mip = iget(dev,ino);
	
	// check if not DIR
	if(((mip->INODE.i_mode)&0040000) != 0040000)
	{
		printf("invalid pathname.\n");
		iput(mip);
		return 1;
	}

	// check if its not busy
	if(mip->refCount>1)
	{
		printf("DIR is being used.\n");
		iput(mip);
		return 1;
	}

	// check if its not empty
	if(!isEmpty(mip))
	{
		printf("DIR not empty\n");
		iput(mip);
		return 1;
	}

	// deallocate its block and inode
	for(i = 0; i <= 11; i++)
	{
		if(mip->INODE.i_block[i] == 0)
			continue;
		bdealloc(mip->dev, mip->INODE.i_block[i]);
	}
	idealloc(mip->dev,mip->ino);
	iput(mip); // clears mip->refCount = 0

	// get parent dir's inode and minode
	findino(mip,&ino,&parent);
	pip = iget(mip->dev,parent);

	// get the name of the parent
	if(findparent(pathname))
		my_name = basename(pathname);
	else
	{
		my_name = (char *)malloc((strlen(pathname)+1)*sizeof(char));
		strcpy(my_name,pathname);
	}

	// remove child's entry from parent directory
	rm_child(pip,my_name);
	pip->INODE.i_links_count--; // decrement pip's link count
	pip->INODE.i_atime = pip->INODE.i_mtime = time(0L); // touch pips time fields
	pip->dirty = 1; // mark as dirty
	iput(pip); //cleanup
	return 0; // return success :)
}

int rm_child(MINODE *parent, char *my_name)
{
	int i, j, total_length, next_length, removed_length, previous_length;
	DIR *dNext;
	char buf[BLOCK_SIZE], namebuf[256], temp[BLOCK_SIZE], *cp, *cNext;

	// search parent inode's data blocks for the entry of my_name
	for(i = 0; i <= 11; i++)
	{
		if(parent->INODE.i_block[i])
		{
			get_block(parent->dev, parent->INODE.i_block[i], buf);
			dp = (DIR *)buf;
			cp = buf;
			j = 0;
			total_length = 0;
			while(cp < &buf[BLOCK_SIZE])
			{
				strncpy(namebuf, dp->name, dp->name_len);
				namebuf[dp->name_len] = 0;
				total_length += dp->rec_len;

				// found my_name? then we must erase it! no one must ever know!
				if(!strcmp(namebuf, my_name))
				{
					// if not first entry in data block
					if(j)
					{
						// if my_name is the last entry in the data block...
						if(total_length == BLOCK_SIZE)
						{
							removed_length = dp->rec_len;
							cp -= previous_length;
							dp =(DIR *)cp;
							dp->rec_len += removed_length;
							put_block(parent->dev, parent->INODE.i_block[i], buf);
							parent->dirty = 1;
							return 0;
						}

						// otherwise, we must move all entries after this one left
						removed_length = dp->rec_len;
						cNext = cp + dp->rec_len;
						dNext = (DIR *)cNext;
						while(total_length + dNext->rec_len < BLOCK_SIZE)
						{
							total_length += dNext->rec_len;
							next_length = dNext->rec_len;
							dp->inode = dNext->inode;
							dp->rec_len = dNext->rec_len;
							dp->name_len = dNext->name_len;
							strncpy(dp->name, dNext->name, dNext->name_len);
							cNext += next_length;
							dNext = (DIR *)cNext;
							cp+= next_length;
							dp = (DIR *)cp;
						}
						dp->inode = dNext->inode;
						// add removed rec_len to the last entry of the block
						dp->rec_len = dNext->rec_len + removed_length;
						dp->name_len = dNext->name_len;
						strncpy(dp->name, dNext->name, dNext->name_len);
						put_block(parent->dev, parent->INODE.i_block[i], buf); // save
						parent->dirty = 1;
						return 0;
					}
					// if first entry in a data block
					else
					{
						// deallocate the data block and modify the parent's file size
						bdealloc(parent->dev, parent->INODE.i_block[i]);
						memset(temp, 0, BLOCK_SIZE);
						put_block(parent->dev, parent->INODE.i_block[i], temp);
						parent->INODE.i_size -= BLOCK_SIZE;
						parent->INODE.i_block[i] = 0;
						parent->dirty = 1;
						return 0;
					}
				}
				j++;
				previous_length = dp->rec_len;
				cp+=dp->rec_len;
				dp = (DIR *)cp;
			}
		}
	}
	return 1;
}
