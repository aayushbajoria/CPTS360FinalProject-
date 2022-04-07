int Unkink()
{
	return do_unlink(0);
}
int do_unlink(int forcerm)
{
	MINODE *mip;
	int dev, i, m, l, *k, *j, *t, block[15], symfile = 0;
	char path[256], parent[256], child[256], buf[BLOCK_SIZE], buf2[BLOCK_SIZE];
	unsigned long inumber;
	
	if (pathname[0] == 0)
	{
		printf("No pathname for a directory to remove given!\n");
		return 1;
	}

	// absolute vs. relative paths
	if(pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	// get the path's inumber
	inumber = getino(&dev, pathname);
	if(!inumber)
		return 1;

	// load the path into memory
	mip = iget(dev, inumber);

	// verify that we're dealing with a file, not a DIR
	if(((mip->INODE.i_mode) & 0040000) == 0040000)
	{
		printf("%s is not a REG file.\n", pathname);
		iput(mip);
		return 1;
	}
	
	// check if its a symlink (so we can make some special exceptions on what we're doing during deallocation)
	if(((mip->INODE.i_mode) & 0xA1FF) == 0xA1FF)
	{
		symfile = 1;
		iput(mip);
	}

	// decrement INODE's i_links_count by 1
	mip->INODE.i_links_count--;

	// if i_links_count == 0 ==> remove the file
	if(mip->INODE.i_links_count == 0 || forcerm == 1)
	{
		// setup the data blocks so we can deallocate them
		for(i = 0; i < 15; i++)
			block[i] = mip->INODE.i_block[i];

		// if we're *not* dealing with a symlink file, then we need to deallocate inode blocks
		if (!symfile)
		{
			// deallocate the direct blocks
			for(i = 0; i <= 11; i++)
			{
				if(block[i])
					bdealloc(mip->dev, block[i]);
			}

			// deallocate the indirect blocks
			if(block[12])
			{
				get_block(mip->dev, block[12], buf);
				k = (int *)buf;
				for(i = 0; i < 256; i++)
				{
					if(*k)
						bdealloc(mip->dev, *k);
					k++;
				}
			}

			// deallocate the double indirect blocks
			if(block[13])
			{
				get_block(mip->dev, block[13], buf);
				t = (int *)buf;
				for(i = 0; i < 256 ; i++)
				{
					if(*t)
					{
						get_block(mip->dev, *t, buf2);
						j = (int *)buf2;
						for(m = 0; m < 256; m++)
						{
							if(*j)
								bdealloc(mip->dev, *j);
							j++;
						}
					}
					t++;
				}
			}
		}

		// deallocate its INODE
		idealloc(mip->dev, mip->ino);
	}
	mip->dirty = 1;
	iput(mip);

	// get the parent minode so we can remove it from the parent directory
	if(findparent(pathname))
	{
		strcpy(parent, dirname(pathname));
		strcpy(child, basename(pathname));
		inumber = getino(&dev, parent);
		mip = iget(dev, inumber);
	}
	else
	{
		strcpy(parent, ".");
		strcpy(child, pathname);
		mip = iget(running->cwd->dev, running->cwd->ino);
	}
 
	rm_child(mip, child); // remove the file from the parent directory
	mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
	mip->dirty = 1;
	iput(mip);
	
	return 0;
}
