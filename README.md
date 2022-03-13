# file_system
Linux File System Simulator


Part A



`Read`

  Accepts offset and size from the user to read the file.
  Reads the file starting from offset to the length of size mentioned by the user. First calculates the block from where the offset should start and stores it in variable - “offset_starts_here”. Next, it starts reading the blocks from where the offset starts.
  



`Link`

  Link Files to other files. Check if the source File exists and Check if the Destination File already exists. Increase the link count of the source file. 
  

`Remove File`

  To remove the file, we check if the file exits in the current directory and we then check that the input is a file and not a directory.
  To actually remove the file, fetch the file’s location in the current directory and replace it with the last file in the directory.
  Then check if the link count of the file is 1. If yes, then release the blocks and set the inode bit of the file to 0 again.
  
  
Part B


`Directory Make`

  For directory make, check if the directory name already exists and inodes are free. Then enter the inode information for the directory like the inode[inodeNum].type = directory, block count, size, etc. Then navigate to that directory and then increase the directory number entry of the newly created directory to 2. In the 1st entry enter the name as “.” meaning the current directory and in the 2nd entry enter the name as “..” meaning the parent directory. Once this is done, then navigate back to the parent directory.
  
`Directory Remove`
  For Directory remove, navigate to that directory and check if the directory is empty. If it is empty, then delete the directory and free the blocks and set the inode bit to 0. If the directory is not empty, check navigate to that directory using the function nav_dir then check for files and delete all the files in the directory and free its block count and inodes. Then check for the sub directories and check if the directories are empty. Continue this recursively until there is no new sub-directory. 
  Then backtrack to the previous directory using the iteration variable and recursively check for any left files or directories. Once this is done then come back to the parent directory where user was previously there, and then now simply delete the mentioned directory since it is now empty.


`Directory Change`

  cd <dir_name>
  cd ..
  cd .
  cd /
  1. cd <dir_name>
  To change the directory, we first check if the Directory exists. Then fetch the inode number of this directory. Using the inode number, get the inode block. Then write the Current Directory, change the curDirBlock to the block. And read the current directory.
  2. cd ..
  To go to the parent directory of the current directory use cd ..
  This first fetches the inode of the parent directory which is the 2nd directory entry of the current directory. Then get the block of this inode. Then write the Current Directory, change the curDirBlock to the block. And read the current directory.
  3. cd .
  cd . is simply the current directory itself.
  4. cd /
  To easily navigate back to the initial root directory use cd / 
  It uses inode 0 since to get the block of the root directory.

`Test Cases:- `

  Tested on Odin for 
  File read. (checked if the read takes place starting from the offset and also that the length of the string is same as the size. Also checked the invalid cases)
  File remove (checked that blocks and inode become free)
  Hard link (checked that hard link is created and link count increases. Also checked that if 1 hard link or file is deleted and another is not then blocks remain the same on file remove. If the link is 1 then free the inode and blocks.)

`Dir make.`

  Change Directory
  Remove Directory (checked that the directory and all the subdirectories and files within it are removed. Also the blocks and inodes are released)

`REMOVE`

  I implemented remove in following manner:
  Check if the directory is empty using the current directory numenrty. 
  If the numentry is 2 then the directory is empty then delete the directory. 
  If th directory is not empty, then check the directory count and consecutively delete any files and directories that exists in that directory. 
