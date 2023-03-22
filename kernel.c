// P3 kernel.c solution
// Author: Brian Law
//P5 work by Rahul Vudaru

#define MAIN
#include "proc.h"


int mod(int dividend, int divisor);
int printString(char *str);
int readChar();
int readString(char *buf);
int readSector(char *buf, int absSector);
int handleInterrupt21(int ax, int bx, int cx, int dx);

// P3 functions
int readFile(char *filename, char *buf);
int executeProgram(char* name, int segment);
void terminate();

// P4 functions
int writeSector(char *buffer, int sector);
int deleteFile(char *fname);
int writeFile(char *fname, char *buffer, int sectors);

// P5 functions
void handleTimerInterrupt(int segment, int stackPointer);
void yield();
void kill();

int main(void) {
  	
/*
	// P2 testing
	char line[80]; // A buffer of size 80
	char buffer[512];
	char ch[1];

	// Test printString and readString
	printString("Enter a line: ");
	readString(line);
	printString("\n\r");
	printString(line);
	
	// Test readSector
	readSector(buffer, 30);
	printString(buffer);

	// Load and test interrupt 21
	makeInterrupt21();
	interrupt(0x21, 0x00, "Prompt$ ", 0, 0); // display prompt
	interrupt(0x21, 0x11, ch, 0, 0); // read char
	line[0] = ch[0];
	line[1] = 0x00;
	interrupt(0x21, 0x00, line, 0, 0); // print string
	interrupt(0x21, 0x01, line, 0, 0); // read string
	interrupt(0x21, 0x00, line, 0, 0); // print string
*/


	// P3 testing
/*
	char buffer[13312]; // the maximum size of a file
	makeInterrupt21();
	// read the file into buffer 
	interrupt(0x21, 0x03, "messag", buffer, 0);
	// print the file contents to the console
	interrupt(0x21, 0x00, buffer, 0, 0);

	interrupt(0x21, 0x04, "uprog1", 0x2000, 0);
	interrupt(0x21, 0x00, "Done!\n\r", 0, 0);

	interrupt(0x21, 0x04, "uprog2", 0x2000, 0);
	interrupt(0x21, 0x00, "Done!\n\r", 0, 0);
*/
/*
	// P4 testing
	char longFile[13312];
	int i=0;
	for (; i<13312; i++) {
		longFile[i] = 'a';
	}


	makeInterrupt21();
	writeSector("Testing 1-2-3", 2879);
	deleteFile("messag");
	writeFile("nessag", longFile, 5);
	writeFile("nessag", "Testing 1-2-3\0", 1);
	interrupt(0x21, 0x04, "shell", 0x2000, 0);


	while(1){}
*/

	// P5 testing
	makeInterrupt21();
	handleTimerInterrupt(0,0);
	interrupt(0x21, 0x04, "shell", 0x2000, 0);

	while(1){};
}

/** Calculate dividend % divisor. Handles negative numbers properly.
*/
int mod(int dividend, int divisor) {
	// Deal with negative divisor.
	if (divisor < 0) {
		divisor = divisor * -1;
	}
	// This should probably throw an exception... but we're the OS!
	else if (divisor == 0) {
		return -1;
	}

	// Positive dividend
	if (dividend >= 0) {
		while (dividend >= divisor) {
			dividend = dividend - divisor;
		}
	}
	// Negative dividend
	else {
		while (dividend < 0) {
			dividend = dividend + divisor;
		}
	}

	return dividend;
}


/** 
Print the null-terminated C-string at memaddr str to the screen.
Return the number of characters printed.
*/
int printString(char *str) {
	// Counter for characters being printed.
	int i = 0;

	// Print until null-character.
	while (str[i] != 0x00) {
		interrupt(0x10, 0x0E * 256 + str[i], 0, 0, 0);
		i++;
	}
	return i;
}

/** Read a single character from standard input.
*/
int readChar() {
	return interrupt(0x16, 0, 0, 0, 0);
}

/** Read a string from input into the memaddr buf until the user presses Enter.
*/
int readString(char *buf) {
	// Counter for # characters read
	int i = 0;

	// Initialize by reading first character.	
	char c = readChar();

	// Continue until read character is a newline.
	while (c != 0x0d) {
		// Dealing with backspaces
		if (c == 0x08) {

			// Do not backspace past starting point.
			if (i != 0) {
				// Back up the cursor
				interrupt(0x10, 0xe*256 + c, 0, 0, 0);

				// Print blank to remove previous character
				interrupt(0x10, 0xe*256, 0, 0, 0);

				// Back cursor up again after printing!
				interrupt(0x10, 0xe*256 + c, 0, 0, 0);

				// Remove last read character from buffer
				i--;
				buf[i] = 0x00;
			}
		}
		// Normal characters go into the buffer and get echoed
		else {
			buf[i] = c;
			interrupt(0x10, 0xe*256 + c, 0, 0, 0);
			i++;

		}

		// Read next char for next loop iteration
		c = readChar();
	}

	// Terminate buffer with null character
	buf[i] = 0x00;
	return i;
}

/** Read in the selected disk sector into the memaddr at buf. Return 1.
*/
int readSector(char *buf, int absSector) {
	int relSector = mod(absSector,18) + 1;
	int head = mod(absSector / 18, 2);
	int track = absSector / 36;
	interrupt(0x13,0x02*256 + 1, buf, track*256 + relSector, head*256 + 0x00);
	return 1;
}

/** Read in a file into the provided character buffer.
filename should be a char buffer of maximum length 7 with the name of the file to be loaded
buf should be a char buffer with enough room for the whole file (512 bytes * 26 sectors =
13312 bytes - no need for null character, since we're treating this as binary, not ASCII)
Returns the number of sectors read.
*/
int readFile(char *filename, char *buf) {
	char diskDirectory[512];
	int filenameLocation = 0;
	int sectorsRead = -1;

	// Read in the disk directory and find where our file is in the disk directory.
	readSector(diskDirectory, 2);
	filenameLocation = findFile(filename, diskDirectory);

	// If the file was found in the disk directory...
	if (filenameLocation != -1) {
		// Read in each sector one at a time into the buffer, offsetting by 512 each time.
		for (sectorsRead = 0; diskDirectory[filenameLocation + 6 + sectorsRead] != 0 && sectorsRead < 26; sectorsRead++) {
			readSector(buf + (512 * sectorsRead), diskDirectory[filenameLocation + 6 + sectorsRead]);
		}
        }

        return sectorsRead;
}

// Execute the program with the filename name by loading it into the specified segment of memory.
// Return -1 if program not found, -2 if segment invalid.
int executeProgram(char* filename, int segment) {
	char fileBuffer[13312];
	int sectorsRead = 0;
	int sectorsLoaded = 0;
	int bytesLoaded = 0;
	struct PCB *pcb; //set name of struct to filename
	int pcbSegment;

	// Invalid segment numbers return -2
	if (segment == 0x0000 || segment == 0x1000 || segment >= 0xA000 || mod(segment, 0x1000) != 0) {
		return -2;
	}
	
	// Read the program (file) to be executed
	sectorsRead = readFile(filename, fileBuffer);

	// File read error, return -1.
	if (sectorsRead <= 0 || sectorsRead > 26) {
		return -1;
	}

	// Put each byte of the program into the specified segment, one byte at a time.
	for (sectorsLoaded=0; sectorsLoaded < sectorsRead; sectorsLoaded++) {
		for (bytesLoaded = 0; bytesLoaded < 512; bytesLoaded++) {
			putInMemory(segment, sectorsLoaded * 512 + bytesLoaded, fileBuffer[sectorsLoaded * 512 + bytesLoaded]);
		}
	}
	
	//setting pcb as starting
	pcb = getFreePCB();
	//setting memory segment as loaded
	pcb.segment = getFreeMemorySegment();
	//set stack pointer to 0xFF00
	pcb.stackPointer = 0xFF00;	

	// MOVE ZIG
	initializeProgram(segment);
}

// Terminate a program and execute the shell again.
void terminate() {
	resetSegments();
	executeProgram("shell", 0x2000);
}

/** Write to the specified disk sector the data buffer. Will read-overrun buffer if buffer is
not sized properly for sector number (512 * sector bytes). Return 1.
*/
int writeSector(char *buffer, int sector) {
	int relSector = mod(sector,18) + 1;
	int head = mod(sector / 18, 2);
	int track = sector / 36;
	interrupt(0x13,0x03*256 + 1, buffer, track*256 + relSector, head*256 + 0x00);
	return 1;
}

/* Helper function to find a filename in the disk directory. Disk directory contents should be
passed in as a 512-char buffer from calling function, to minimize sector reads. 
Return the byte number of the file's disk directory entry if found, -1 if not found.
*/
int findFile(char *filename, char* diskDirectory) {
	int fileEntry = 0;
	int filenameLocation = 0;
	int filenameChars = 0;
	int charMatch = 0;

	// Loop through all the file entries in the file directory.
	for (fileEntry = 0; fileEntry < 16; fileEntry++) {

		// Calculate the offset of the filename for this entry.
		filenameLocation = fileEntry * 32;

		// Loop over the 6 characters in the filename.		
		for (filenameChars = 0; filenameChars < 6; filenameChars++) {

			// If next character in the two filenames don't match, abort the search.
			if (filename[filenameChars] != diskDirectory[filenameLocation + filenameChars]) {
				charMatch = 0;		// Indicates no filename match.
				filenameChars = 6;	// Stops for-loop.
			}
			else {
				// If both filenames end "early," filename match!
				if (filename[filenameChars] == 0) {
					charMatch = 6;		// Indicates filename match.
					filenameChars = 6;	// Stops for-loop
				}
				// Otherwise count a character match.
				else {
					charMatch++;
				}
			}
		}

		// If filenames matched, terminate file entry searching.
		if (charMatch == 6) {
			fileEntry = 16;
		}
	}

	// If filenames matched, return the offset of the desired file entry. 
	if (charMatch == 6) {
		return filenameLocation;
	}
	// File not found.
	else {
		return -1;
	}
}

/** Delete the specified file. Return 1 if successful, -1 if unsuccessful.
*/
int deleteFile(char *fname) {
	char diskMap[512];
	char diskDirectory[512];
	int sectorCount = 0;
	int sectorNum = 0;
	int filenameLocation = 0;

	// Read in disk directory.
	readSector(diskDirectory, 2);

	// Try to find the file.
	filenameLocation = findFile(fname, diskDirectory);

	// If file not found, just return -1.
	if (filenameLocation == -1) {
		return -1;
	}

	// Read in disk map here, for efficiency.
	readSector(diskMap, 1);

	// Read in the file's sectonr numbers, and zero out each corresponding sector entry in the disk map.
	for (sectorCount = 0; diskDirectory[filenameLocation + 6 + sectorCount] != 0 && sectorCount < 26; sectorCount++) {
		sectorNum = diskDirectory[filenameLocation + 6 + sectorCount];
		diskMap[sectorNum] = 0;
		
	}

	// Tombstone the file's entry in the disk directory.
	diskDirectory[filenameLocation] = 0;

	// Write back to disk map and disk directory.
	writeSector(diskMap, 1);
	writeSector(diskDirectory, 2);
}

int writeFile(char *fname, char *buffer, int sectors) {
	char diskMap[512];
	char diskDirectory[512];
	char emptyFile[1];
	int filenameLocation = 0;
	int filenameIndex = 0;
	int diskMapIndex = 0;
	int freeSectorCount = 0;
	int freeSectors[26];
	int sectorsWritten = 0;

	// Read in disk map and directory.
	readSector(diskMap, 1);
	readSector(diskDirectory, 2);

	// Try to find the file.
	filenameLocation = findFile(fname, diskDirectory);

	// File was found, overwrite it.
	if (filenameLocation != -1) {
		// Examine the sectors currently occupied by the file and store their numbers in
		// freeSectors, to be overwritten below.
		for (freeSectorCount = 0; diskDirectory[filenameLocation + 6 + freeSectorCount] != 0 && freeSectorCount < 26; freeSectorCount++) {
			freeSectors[freeSectorCount] = diskDirectory[filenameLocation + 6 + freeSectorCount];
		}

		// If file being overwritten is MORE sectors than overwriting file, free up the
		// unneeded sectors.
		for (; freeSectorCount > sectors; freeSectorCount--) {
			diskMap[freeSectors[freeSectorCount-1]] = 0;
			diskDirectory[filenameLocation + 6 + freeSectorCount-1] = 0;
		}

	}
	// If file not found, write it in.
	else {
		// A hack to find a diskDirectory starting with 0: either empty disk entry
		// or a tombstoned deleted file.
		emptyFile[0] = 0;
		filenameLocation = findFile(emptyFile, diskDirectory);

		// No free disk directory entries to add this file. Return -1 for error code.
		if (filenameLocation == -1) {
			
			return -1;
		}

	}

	// Scan the disk map for more free sectors to write to, if needed.
	for (diskMapIndex = 0; diskMapIndex < 512 && freeSectorCount < sectors; diskMapIndex++) {
		if (diskMap[diskMapIndex] == 0) {
			freeSectors[freeSectorCount] = diskMapIndex;
			freeSectorCount++;
		}
	}
	
	// Write the filename into this position in the disk directory.
	for (filenameIndex = 0; fname[filenameIndex] != 0 && filenameIndex < 6; filenameIndex++) {
		diskDirectory[filenameLocation + filenameIndex] = fname[filenameIndex];
	} 

	// If the filename is less than 6 characters, zero out the remaining characters in
	// the directory entry filename, because delete only tombstones names, and we don't want
	// to "absorb" a leftover filename.
	for (; filenameIndex < 6; filenameIndex++) {
		diskDirectory[filenameLocation + filenameIndex] = 0;
	}

	// Write in sector numbers to the disk directory.
	for (; filenameIndex - 6 < freeSectorCount; filenameIndex++) {
		diskDirectory[filenameLocation + filenameIndex] = freeSectors[filenameIndex - 6];
	}

	// Write in the target sectors now.
	for (; sectorsWritten < sectors && sectorsWritten < freeSectorCount; sectorsWritten++) {
		writeSector(buffer + sectorsWritten * 512, freeSectors[sectorsWritten]);
		diskMap[freeSectors[sectorsWritten]] = 0xff;
	}

	writeSector(diskMap, 1);
	writeSector(diskDirectory, 2);

	// Not enough free sectors available - return -2 for error.
	if (freeSectorCount < sectors) {
		return -2;
	}

	return 1;
}

/** Examine the disk and write a list of the files on disk back to contentsBuffer.
How big should contentsBuffer be? (6 (filename) + 2 (newlines) * 16 (num files) - last newline + null = 127 bytes?)
*/
void directoryContents(char* contentsBuffer) {
	char diskDirectory[512];
	int fileEntry = 0;
	int filenameLocation = 0;
	int filenameChars = 0;
	int charCounter = 0;

	// Read in disk directory.
	readSector(diskDirectory, 2);

	// Loop over every file entry in the file directory (16 in total)
	for (fileEntry = 0; fileEntry < 16; fileEntry++) {
		// Calculate the start position of each filename (if present)
		filenameLocation = fileEntry * 32;

		// If this entry has a filename (not empty, not tombstoned)
		if (diskDirectory[filenameLocation] != 0) {

			// Add newline between entries, but not before the first.
			if (charCounter != 0) {
				contentsBuffer[charCounter] = '\n';
				charCounter++;
				contentsBuffer[charCounter] = '\r';
				charCounter++;
			}

			// Copy over every NON-NULL character in the filename (stop at 6, to avoid sector numbers)
			for (filenameChars = 0; filenameChars < 6 && diskDirectory[filenameLocation + filenameChars] != 0; filenameChars++) {
				contentsBuffer[charCounter] = diskDirectory[filenameLocation + filenameChars];
				charCounter++;
			}
		}
	}

	// Add a null character at the end of the buffer.
	contentsBuffer[charCounter] = 0;
	charCounter++;
}

//for now, just print "tic" and then call returnFromTimer
void handleTimerInterrupt(int segment, int stackPointer) {
	//printString("tic");
	struct PCB *pcb;
	//set pcb segment and pointer to arguments passed by function
	pcb.segment = segment;
	pcb.stackPointer = stackPointer;
	//set pcb to ready and add it to tail of ready queue
	pcb.state = READY;
	addToReady(pcb);
	//remove pcb from head of ready queue and set current one to running
	removeFromReady();
	pcb.state = RUNNING;
	//set running to pcb address
	running = &pcb;
	//run returnFromTimer with segment and stackpointer
	returnFromTimer(segment, stackPointer);
	
}

//surrender the rest of the time slice 
void yield() {
	//idk
}

void showProcesses() {
	//not sure, I'm not sure how to check the process map and such
}

int kill(int segment) {
	
	//im not sure how to check if the segment is already taken but...
	releaseMemorySegment(segment);
	return 1;
	//this atleast solves half of what is expected	

}

/** Interrupt handler for interrupt 21.
*/
int handleInterrupt21(int ax, int bx, int cx, int dx) {
	int returnValue = 1;
	if (ax == 0x00) {
		returnValue = printString(bx);
	}
	else if (ax == 0x11) {
		*((char*)bx) = (char)readChar();
	}
	else if (ax == 0x01) {
		returnValue = readString(bx);
	}
	else if (ax == 0x03) {
		returnValue = readFile(bx, cx);
	}
	else if (ax == 0x04) {
		returnValue = executeProgram(bx, cx);
	}
	else if (ax == 0x05) {
		terminate();
	}
	else if (ax == 0x07) {
		returnValue = deleteFile(bx);
	}
	else if (ax == 0x08) {
		returnValue = writeFile(bx, cx, dx);
	} 
	else if (ax == 0x09) {
		returnValue = yield();
	}
	else if (ax == 0x0A) {
		returnValue = showProcesses();
	}
	else if (ax == 0x0B) {
		returnValue = kill(bx);
	}
	else if (ax = 0xff) {
		directoryContents(bx);
	}
	return returnValue;
}
