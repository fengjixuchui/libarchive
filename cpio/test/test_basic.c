/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD: src/usr.bin/cpio/test/test_basic.c,v 1.4 2008/08/25 06:39:29 kientzle Exp $");

static void
verify_files(const char *target)
{
	struct stat st, st2;
	int r;

	/*
	 * Verify unpacked files.
	 */

	/* Regular file with 2 links. */
	r = lstat("file", &st);
	failure("Failed to stat file %s/file, errno=%d", target, errno);
	assertEqualInt(r, 0);
	if (r == 0) {
		assert(S_ISREG(st.st_mode));
#if defined(_WIN32) && !defined(__CYGWIN__)
		/* Group members bits and others bits do not work. */
		assertEqualInt(0600, st.st_mode & 0700);
#else
		assertEqualInt(0644, st.st_mode & 0777);
#endif
		assertEqualInt(10, st.st_size);
		failure("file %s/file should have 2 links", target);
		assertEqualInt(2, st.st_nlink);
	}

	/* Another name for the same file. */
	r = lstat("linkfile", &st2);
	failure("Failed to stat file %s/linkfile, errno=%d", target, errno);
	assertEqualInt(r, 0);
	if (r == 0) {
		assert(S_ISREG(st2.st_mode));
#if defined(_WIN32) && !defined(__CYGWIN__)
		/* Group members bits and others bits do not work. */
		assertEqualInt(0600, st2.st_mode & 0700);
#else
		assertEqualInt(0644, st2.st_mode & 0777);
#endif
		assertEqualInt(10, st2.st_size);
		failure("file %s/linkfile should have 2 links", target);
		assertEqualInt(2, st2.st_nlink);
		/* Verify that the two are really hardlinked. */
		assertEqualInt(st.st_dev, st2.st_dev);
		failure("%s/linkfile and %s/file should be hardlinked",
		    target, target);
		assertEqualInt(st.st_ino, st2.st_ino);
	}

	/* Symlink */
#if !defined(_WIN32) || defined(__CYGWIN__)
	assertIsSymlink("symlink", "file");
#endif

	/* Another file with 1 link and different permissions. */
	r = lstat("file2", &st);
	failure("Failed to stat file %s/file2, errno=%d", target, errno);
	assertEqualInt(r, 0);
	if (r == 0) {
		assert(S_ISREG(st.st_mode));
		failure("%s/file2: st.st_mode = %o", target, st.st_mode);
#if defined(_WIN32) && !defined(__CYGWIN__)
		/* Execution bit and group members bits and others
		 * bits do not work. */
		assertEqualInt(0600, st.st_mode & 0700);
#else
		assertEqualInt(0777, st.st_mode & 0777);
#endif
		assertEqualInt(10, st.st_size);
		failure("file %s/file2 should have 1 link", target);
		assertEqualInt(1, st.st_nlink);
	}

	/* dir */
	r = lstat("dir", &st);
	if (r == 0) {
		assertEqualInt(r, 0);
		assert(S_ISDIR(st.st_mode));
		failure("%s/dir: st.st_mode = %o", target, st.st_mode);
#if defined(_WIN32) && !defined(__CYGWIN__)
		assertEqualInt(0700, st.st_mode & 0700);
#else
		assertEqualInt(0775, st.st_mode & 0777);
#endif
	}
}

static void
basic_cpio(const char *target,
    const char *pack_options,
    const char *unpack_options,
    const char *se)
{
	int r;

	if (!assertEqualInt(0, mkdir(target, 0775)))
	    return;

	/* Use the cpio program to create an archive. */
	r = systemf("%s -o %s < filelist >%s/archive 2>%s/pack.err",
	    testprog, pack_options, target, target);
	failure("Error invoking %s -o %s", testprog, pack_options);
	assertEqualInt(r, 0);

	assertChdir(target);

	/* Verify stderr. */
	failure("Expected: %s, options=%s", se, pack_options);
	assertTextFileContents(se, "pack.err");

	/*
	 * Use cpio to unpack the archive into another directory.
	 */
	r = systemf("%s -i %s< archive >unpack.out 2>unpack.err",
	    testprog, unpack_options);
	failure("Error invoking %s -i %s", testprog, unpack_options);
	assertEqualInt(r, 0);

	/* Verify stderr. */
	failure("Error invoking %s -i %s in dir %s", testprog, unpack_options, target);
	assertTextFileContents(se, "unpack.err");

	verify_files(target);

	assertChdir("..");
}

static void
passthrough(const char *target)
{
	int r;

	if (!assertEqualInt(0, mkdir(target, 0775)))
		return;

	/*
	 * Use cpio passthrough mode to copy files to another directory.
	 */
	r = systemf("%s -p %s <filelist >%s/stdout 2>%s/stderr",
	    testprog, target, target, target);
	failure("Error invoking %s -p", testprog);
	assertEqualInt(r, 0);

	assertChdir(target);

	/* Verify stderr. */
	failure("Error invoking %s -p in dir %s",
	    testprog, target);
	assertTextFileContents("1 block\n", "stderr");

	verify_files(target);
	assertChdir("..");
}

DEFINE_TEST(test_basic)
{
	int fd;
	int filelist;
	int oldumask;

	oldumask = umask(0);

	/*
	 * Create an assortment of files on disk.
	 */
	filelist = open("filelist", O_CREAT | O_WRONLY, 0644);

	/* File with 10 bytes content. */
	fd = open("file", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(10, write(fd, "123456789", 10));
	close(fd);
	write(filelist, "file\n", 5);

	/* hardlink to above file. */
	assertMakeHardlink("linkfile", "file");
	write(filelist, "linkfile\n", 9);

	/* Symlink to above file. */
	assertMakeSymlink("symlink", "file");
	write(filelist, "symlink\n", 8);

	/* Another file with different permissions. */
	fd = open("file2", O_CREAT | O_WRONLY, 0777);
	assert(fd >= 0);
	assertEqualInt(10, write(fd, "123456789", 10));
	close(fd);
	write(filelist, "file2\n", 6);

	/* Directory. */
	assertEqualInt(0, mkdir("dir", 0775));
	write(filelist, "dir\n", 4);
	/* All done. */
	close(filelist);

	umask(022);

	/* Archive/dearchive with a variety of options. */
	basic_cpio("copy", "", "", "2 blocks\n");
	basic_cpio("copy_odc", "--format=odc", "", "2 blocks\n");
	basic_cpio("copy_newc", "-H newc", "", "2 blocks\n");
	basic_cpio("copy_cpio", "-H odc", "", "2 blocks\n");
#if defined(_WIN32) && !defined(__CYGWIN__)
	/*
	 * On Windows, symbolic link does not work.
	 * Currentry copying file instead. therefore block size is
	 * different.
	 */
	basic_cpio("copy_ustar", "-H ustar", "", "10 blocks\n");
#else
	basic_cpio("copy_ustar", "-H ustar", "", "9 blocks\n");
#endif
	/* Copy in one step using -p */
	passthrough("passthrough");

	umask(oldumask);
}
