#include <efi.h>
#include <efilib.h>
#include "popcorn.h"

extern EFI_SYSTEM_TABLE *gST;
extern EFI_BOOT_SERVICES *gBS;
extern EFI_RUNTIME_SERVICES *gRT;
extern EFI_HANDLE *ImageHandle;

// Forward declarations
static popf_File* popf__virtfopen(pop_Services* svc, const CHAR16* uefipath, popf_FileMode mode);
static popf_File* popf__uefifopen(pop_Services* svc, const CHAR16* uefipath, popf_FileMode mode, int uefiflags);
static void*      popf__uefifread(popf_File* self);
static int        popf__uefifwrite(popf_File* self, const CHAR16* data);
static int        popf__ueficlose(popf_File* self);

// Fallbacks
static void*   pop_API popf__failread(popf_File* self) { return NULL; }
static int     pop_API popf__failclose(popf_File* self) { return pop_ENOPERM; }
static int     pop_API popf__virtclose(popf_File* self) { return 0; }

// Console write
static int pop_API popft__ttywrite(popf_File* self, const CHAR16* data) {
    gST->ConOut->OutputString(gST->ConOut, data);
    return 0;
}
static void* pop_API popft__ttyread(popf_File* self) {
    EFI_INPUT_KEY Key;
    UINTN Index;
    size_t cap = 128;
    size_t len = 0;
    CHAR16* buf;
    gBS->AllocatePool(EfiLoaderData, cap * sizeof(CHAR16), &buf);

    while (1) {
        // Wait for key
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

        if (Key.UnicodeChar == L'\r') { // Enter pressed
            buf[len] = L'\0';
            gST->ConOut->OutputString(gST->ConOut, L"\r\n"); // echo newline
            return buf;
        } else if (Key.UnicodeChar == L'\b') { // Backspace
            if (len > 0) {
                len--;
                gST->ConOut->OutputString(gST->ConOut, L"\b");
            }
        } else if (Key.UnicodeChar != 0) {
            // Echo character
            CHAR16 tmp[2] = { Key.UnicodeChar, L'\0' };
            gST->ConOut->OutputString(gST->ConOut, tmp);

            // Append to buffer
            if (len + 1 >= cap) {
                cap *= 2;
                CHAR16* newbuf;
                gBS->AllocatePool(EfiLoaderData, cap * sizeof(CHAR16), &newbuf);
                for (size_t i = 0; i < len; i++) newbuf[i] = buf[i];
                gBS->FreePool(buf);
                buf = newbuf;
            }
            buf[len++] = Key.UnicodeChar;
        }
    }
}

// Path normalization + fileopen
// Helper: compare wide strings
static inline int wstrcmp(const CHAR16* a, const CHAR16* b) {
    while (*a && *b) {
        if (*a != *b) return (*a - *b);
        a++; b++;
    }
    return (*a - *b);
}

// Helper: compare offset wide strings
// Compare substrings of wide strings between given offsets
static inline int wostrcmp(
    const CHAR16* a, UINTN as, UINTN ae,
    const CHAR16* b, UINTN bs, UINTN be
) {
    // Move pointers to starting offsets
    a += as;
    b += bs;

    // Compare until one substring ends
    while (as < ae && bs < be) {
        if (*a != *b) {
            return (*a - *b);
        }
        a++; b++;
        as++; bs++;
    }

    // If both substrings ended at the same time, they are equal
    if (as == ae && bs == be) return 0;

    // Otherwise, shorter substring is "less"
    return (as == ae) ? -1 : 1;
}

CHAR16* pop_API popfk_normpath(pop_Services* svc, const CHAR16* path) {
    if (!path) return NULL;

    // Measure lengths
    size_t curdir_len = 0;
    if (svc->curdir) {
        while (svc->curdir[curdir_len]) curdir_len++;
    }
    size_t path_len = 0;
    while (path[path_len]) path_len++;

    size_t total_len = curdir_len + path_len + 2; // +2 for leading '/' and NUL
    CHAR16* result = (CHAR16*)svc->memalloc(svc, total_len * sizeof(CHAR16));
    if (!result) return NULL;

    CHAR16** segments = (CHAR16**)svc->memalloc(svc, (path_len+1) * sizeof(CHAR16*));
    size_t seg_count = 0;

    // Tokenize curdir if relative
    if (path[0] != L'/' && curdir_len > 0) {
        size_t i = 0;
        while (i < curdir_len) {
            while (i < curdir_len && svc->curdir[i] == L'/') i++;
            size_t start = i;
            while (i < curdir_len && svc->curdir[i] != L'/') i++;
            size_t len = i - start;
            if (len > 0) {
                CHAR16* seg = (CHAR16*)svc->memalloc(svc, (len+1)*sizeof(CHAR16));
                for (size_t j=0;j<len;j++) seg[j] = svc->curdir[start+j];
                seg[len]=0;
                segments[seg_count++] = seg;
            }
        }
    }

    // Tokenize input path
    size_t i = 0;
    while (i < path_len) {
        while (i < path_len && path[i] == L'/') i++;
        size_t start = i;
        while (i < path_len && path[i] != L'/') i++;
        size_t len = i - start;
        if (len > 0) {
            CHAR16* seg = (CHAR16*)svc->memalloc(svc, (len+1)*sizeof(CHAR16));
            for (size_t j=0;j<len;j++) seg[j] = path[start+j];
            seg[len]=0;

            if (wstrcmp(seg, L".") == 0) {
                svc->memfree(svc, seg);
            } else if (wstrcmp(seg, L"..") == 0) {
                if (seg_count > 0) {
                    svc->memfree(svc, segments[--seg_count]);
                }
                svc->memfree(svc, seg);
            } else {
                segments[seg_count++] = seg;
            }
        }
    }

    // Rebuild normalized path — always start with exactly one '/'
    CHAR16* out = result;
    *out++ = L'/';
    for (size_t s=0; s<seg_count; s++) {
        if (s > 0) *out++ = L'/';  // add slash before subsequent segments
        size_t j=0;
        while (segments[s][j]) *out++ = segments[s][j++];
        svc->memfree(svc, segments[s]);
    }
    *out = 0;

    svc->memfree(svc, segments);

    // Special case: if no segments, result is just "/"
    if (seg_count == 0) {
        result[0] = L'/';
        result[1] = 0;
    }

    return result;
}

popf_File* pop_API popfk_fileopen(pop_Services* svc, const CHAR16* path, popf_FileMode mode) {
    CHAR16* normpath = popfk_normpath(svc, path);
    if (!normpath) return NULL;

    // Decide backend
    popf_File* f;
    if (wostrcmp(normpath, 0, 6, L"/virt/", 0, 6) == 0) {
        f = popf__virtfopen(svc, normpath, mode);
    } else {
        f = popf__uefifopen(svc, normpath, mode, 0);
    }
    svc->memfree(svc, normpath);
    return f;
}

// Virtual file open
static popf_File* popf__virtfopen(pop_Services* svc, const CHAR16* normpath, popf_FileMode mode) {
    if (wostrcmp(normpath, 6, 10, L"dev/", 0, 4) == 0) {
        if (wostrcmp(normpath, 10, 13, L"con", 0, 3) == 0 && normpath[13] == L'\0') {
            popf_File* f = (popf_File*)svc->memalloc(svc, sizeof(popf_File));
            if (!f) return NULL;
            f->shndl = NULL;
            f->read  = popft__ttyread;
            f->write = popft__ttywrite;
            f->close = popf__virtclose;
            f->svc   = svc;
            f->mode  = mode;
            return f;
        }
    }
    return NULL;
}

// UEFI file read/write/close
static void* pop_API popf__uefifread(popf_File* self) {
    if (!self || !self->shndl) return NULL;
    EFI_FILE_PROTOCOL* File = (EFI_FILE_PROTOCOL*)self->shndl;
    EFI_STATUS Status;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 256;
    EFI_FILE_INFO* FileInfo = NULL;

    // Allocate initial FileInfo buffer
    Status = gBS->AllocatePool(EfiLoaderData, InfoSize, (void**)&FileInfo);
    if (EFI_ERROR(Status)) return NULL;

    // Query file info; handle EFI_BUFFER_TOO_SMALL by reallocating
    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        if (Status == EFI_BUFFER_TOO_SMALL) {
            gBS->FreePool(FileInfo);
            FileInfo = NULL;
            Status = gBS->AllocatePool(EfiLoaderData, InfoSize, (void**)&FileInfo);
            if (EFI_ERROR(Status)) return NULL;
            Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
            if (EFI_ERROR(Status)) {
                gBS->FreePool(FileInfo);
                return NULL;
            }
        } else {
            gBS->FreePool(FileInfo);
            return NULL;
        }
    }

    // FileSize is in bytes
    UINTN FileSizeBytes = (UINTN)FileInfo->FileSize;
    gBS->FreePool(FileInfo);
    FileInfo = NULL;

    if (FileSizeBytes == 0) {
        // Return an empty buffer consistent with mode:
        if (self->mode.bytes) {
            void* empty = self->svc->memalloc(self->svc, 0);
            return empty;
        } else {
            // allocate one CHAR16 for NUL
            CHAR16* empty = (CHAR16*)self->svc->memalloc(self->svc, sizeof(CHAR16));
            if (!empty) return NULL;
            empty[0] = 0;
            return (void*)empty;
        }
    }

    // Allocate and read
    if (self->mode.bytes) {
        // Raw bytes: allocate exactly FileSizeBytes
        void* mem = self->svc->memalloc(self->svc, (unsigned int)FileSizeBytes);
        if (!mem) return NULL;
        UINTN ReadSize = FileSizeBytes;
        Status = File->Read(File, &ReadSize, mem);
        if (EFI_ERROR(Status)) {
            self->svc->memfree(self->svc, mem);
            return NULL;
        }
        // Return raw buffer (caller must know length from context or FileInfo)
        return mem;
    } else {
        // Text mode: allocate FileSizeBytes + sizeof(CHAR16) for NUL termination
        UINTN allocBytes = FileSizeBytes + sizeof(CHAR16);
        CHAR16* mem = (CHAR16*)self->svc->memalloc(self->svc, (unsigned int)allocBytes);
        if (!mem) return NULL;
        UINTN ReadSize = FileSizeBytes;
        Status = File->Read(File, &ReadSize, (void*)mem);
        if (EFI_ERROR(Status)) {
            self->svc->memfree(self->svc, mem);
            return NULL;
        }

        // NUL-terminate (ReadSize is bytes)
        size_t chars = ReadSize / sizeof(CHAR16);
        mem[chars] = 0;

        // If BOM present and not bytes mode, shift left in-place so caller can free original pointer
        if (chars > 0 && mem[0] == 0xFEFF) {
            for (size_t i = 0; i < chars; i++) {
                mem[i] = mem[i + 1];
            }
            mem[chars - 1] = 0;
        }

        return (void*)mem; // caller casts to CHAR16*
    }
}

// Write: only write BOM when creating a new file (mode.create true)
static int pop_API popf__uefifwrite(popf_File* self, const CHAR16* data) {
    if (!self || !self->shndl || !data) return pop_ENOPERM;
    EFI_FILE_PROTOCOL* File = (EFI_FILE_PROTOCOL*)self->shndl;
    UINTN datalen = 0;
    while (data[datalen]) datalen++;
    UINTN size = datalen * sizeof(CHAR16);

    EFI_STATUS Status;

    // Write BOM only if file was opened for create (caller must open with create to get BOM)
    if (!self->mode.bytes && self->mode.create) {
        CHAR16 bom = 0xFEFF;
        UINTN bomSize = sizeof(CHAR16);
        Status = File->Write(File, &bomSize, &bom);
        if (EFI_ERROR(Status)) return pop_ENOPERM;
    }

    Status = File->Write(File, &size, (void*)data);
    return EFI_ERROR(Status) ? pop_ENOPERM : 0;
}


static int pop_API popf__ueficlose(popf_File* self) {
    EFI_FILE_PROTOCOL* File = (EFI_FILE_PROTOCOL*)self->shndl;
    EFI_STATUS Status = File->Close(File);
    return EFI_ERROR(Status) ? pop_ENOPERM : 0;
}

static popf_File* pop_API popf__uefifopen(pop_Services* svc, const CHAR16* normpath, popf_FileMode mode, int uefiflags) {
    // Convert '/' to '\'
    size_t len = 0;
    while (normpath[len]) len++;
    CHAR16* uefipath = (CHAR16*)svc->memalloc(svc, (len+1) * sizeof(CHAR16));
    for (size_t i = 0; i < len; i++) {
        uefipath[i] = (normpath[i] == L'/') ? L'\\' : normpath[i];
    }
    uefipath[len] = 0;

    EFI_STATUS Status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs;
    EFI_FILE_PROTOCOL* Root, *File;

    EFI_LOADED_IMAGE* LoadedImage;
    Status = gST->BootServices->HandleProtocol(
        ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) { svc->errcode = pop_EUEFIPR; return NULL; }

    Status = gST->BootServices->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (void**)&Fs);
    if (EFI_ERROR(Status)) { svc->errcode = pop_EUEFIPR; return NULL; }

    Fs->OpenVolume(Fs, &Root);
    Status = Root->Open(Root, &File, uefipath, (mode.read   ? EFI_FILE_MODE_READ   : 0) | 
                                               (mode.write  ? EFI_FILE_MODE_WRITE  : 0) | 
                                               (mode.create ? EFI_FILE_MODE_CREATE : 0) |
                                               uefiflags, 0);
    if (EFI_ERROR(Status)) { svc->errcode = pop_ENOENTY; return NULL; }

    popf_File* f = (popf_File*)svc->memalloc(svc, sizeof(popf_File));
    f->shndl = (void*)File;
    f->read  = popf__uefifread;
    f->write = popf__uefifwrite;
    f->close = popf__ueficlose;
    f->svc   = svc;
    f->mode  = mode;
    svc->memfree(svc, uefipath);
    return f;
}

BOOL popfk_direxists(pop_Services* svc, const CHAR16* path) {
    CHAR16* normpath = popfk_normpath(svc, path);
    if (!normpath) return FALSE;

    BOOL res = FALSE;

    // Virtual directories
    if (wstrcmp(normpath, L"/virt") == 0) {
        res = TRUE;
    } else if (wstrcmp(normpath, L"/virt/dev") == 0) {
        res = TRUE;
    } else {
        // Try UEFI open with directory check
        popf_File* f = popf__uefifopen(svc, normpath, (popf_FileMode){ .read = 1 }, 0);
        if (f) {
            EFI_FILE_PROTOCOL* File = (EFI_FILE_PROTOCOL*)f->shndl;
            EFI_STATUS Status;
            UINTN InfoSize = sizeof(EFI_FILE_INFO) + 256;
            EFI_FILE_INFO* FileInfo = NULL;
        
            Status = gBS->AllocatePool(EfiLoaderData, InfoSize, (void**)&FileInfo);
            if (!EFI_ERROR(Status)) {
                Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
                if (EFI_ERROR(Status) && Status == EFI_BUFFER_TOO_SMALL) {
                    // Reallocate to the required size and try again
                    gBS->FreePool(FileInfo);
                    FileInfo = NULL;
                    Status = gBS->AllocatePool(EfiLoaderData, InfoSize, (void**)&FileInfo);
                    if (!EFI_ERROR(Status)) {
                        Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
                    }
                }
        
                if (!EFI_ERROR(Status)) {
                    if (FileInfo->Attribute & EFI_FILE_DIRECTORY) {
                        res = TRUE;
                    } else {
                        res = FALSE;
                    }
                }
                if (FileInfo) gBS->FreePool(FileInfo);
            }
        
            f->close(f);
        }
    }

    svc->memfree(svc, normpath);
    return res;
}

popl_Node* popfk_dirlist(pop_Services* svc, const CHAR16* path) {
    CHAR16* npath = popfk_normpath(svc, path);
    
    if (wstrcmp(npath, L"/virt") == 0) {
        popl_Node* n = svc->slist->newnode(svc->slist, NULL);
        svc->slist->append(svc->slist, n, L"dev");
        svc->memfree(svc, npath);
        return n;
    }
    
    if (wstrcmp(npath, L"/virt/dev") == 0) {
        popl_Node* n = svc->slist->newnode(svc->slist, NULL);
        svc->slist->append(svc->slist, n, L"con");
        svc->memfree(svc, npath);
        return n;
    }
    
    // Normalize and open directory
    popf_File* f = svc->fileopen(svc, npath, (popf_FileMode){ .read = 1 });
    if (!f) {
        svc->errcode = pop_ENOENTY;
        return NULL;
    }

    EFI_FILE_PROTOCOL* Dir = (EFI_FILE_PROTOCOL*)f->shndl;
    EFI_STATUS Status;
    UINTN InfoSize;
    EFI_FILE_INFO* FileInfo;
    
    popl_Node* n = svc->slist->newnode(svc->slist, NULL);

    // Iterate entries
    while (1) {
        InfoSize = sizeof(EFI_FILE_INFO) + 256; // initial guess
        Status = gBS->AllocatePool(EfiLoaderData, InfoSize, (void**)&FileInfo);
        if (EFI_ERROR(Status)) {
            svc->errcode = pop_ENOPERM;
            break;
        }
    
        Status = Dir->Read(Dir, &InfoSize, FileInfo);
        if (EFI_ERROR(Status) || InfoSize == 0) {
            gBS->FreePool(FileInfo);
            break; // end of directory
        }
    
        // Copy FileName into svc-managed memory
        int namelen = 0;
        while (FileInfo->FileName[namelen]) namelen++;
        CHAR16* namecopy = svc->memalloc(svc, (namelen + 1) * sizeof(CHAR16));
        for (int i = 0; i < namelen; i++) namecopy[i] = FileInfo->FileName[i];
        namecopy[namelen] = 0;
    
        if ((wstrcmp(namecopy, L"..") != 0) && (wstrcmp(namecopy, L".") != 0)) {
            svc->slist->append(svc->slist, n, namecopy);
        } else {
            svc->memfree(svc, namecopy);
        }
    
        gBS->FreePool(FileInfo);
    }
    
    if (wstrcmp(npath, L"/") == 0) {
        svc->slist->append(svc->slist, n, L"virt");
    }

    f->close(f);
    svc->memfree(svc, npath);
    return n;
}