#include <efi.h>
#include <efilib.h>
#include <elf.h>

typedef unsigned long long size_t;

typedef struct {
	void *address;
	size_t size;
	unsigned width;
	unsigned height;
	unsigned pixelsPerScanline;
} FRAMEBUFFER;

typedef struct {
	unsigned char magic[2];
	unsigned char mode;
	unsigned char size;
} PSF1_HEADER;

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

typedef struct {
	PSF1_HEADER *header;
	void *glyphs;
} PSF1_FONT;

FRAMEBUFFER f;
FRAMEBUFFER *InitializeGOP() {
	EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_STATUS status;

	status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void**)&gop);
	if(EFI_ERROR(status)) {
		Print(L"Unable to locate GOP.\n\r");
		return NULL;
	} else Print(L"GOP located successfully.\n\r");

	f.address = (void*)gop->Mode->FrameBufferBase;
	f.size = gop->Mode->FrameBufferSize;
	f.width = gop->Mode->Info->HorizontalResolution;
	f.height = gop->Mode->Info->VerticalResolution;
	f.pixelsPerScanline = gop->Mode->Info->PixelsPerScanLine;
	return &f;
}

EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable){
	EFI_FILE* LoadedFile;

	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	SystemTable->BootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
	SystemTable->BootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);

	if (Directory == NULL){
		FileSystem->OpenVolume(FileSystem, &Directory);
	}

	EFI_STATUS s = Directory->Open(Directory, &LoadedFile, Path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
	if (s != EFI_SUCCESS){
		return NULL;
	}
	return LoadedFile;
}

PSF1_FONT *LoadPSF1Font(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
	EFI_FILE *font = LoadFile(Directory, Path, ImageHandle, SystemTable);
	if(!font) return NULL;

	PSF1_HEADER *header;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF1_HEADER), (void**)&header);
	UINTN size = sizeof(PSF1_HEADER);
	font->Read(font, &size, header);

	if(header->magic[0] != PSF1_MAGIC0 || header->magic[1] != PSF1_MAGIC1) return NULL;

	UINTN glyphsSize;
	if(header->mode) glyphsSize = header->size * 512;
	else glyphsSize = header->size * 256;

	void *glyphs;
	{
		font->SetPosition(font, sizeof(PSF1_HEADER));
		SystemTable->BootServices->AllocatePool(EfiLoaderData, glyphsSize, (void**)&glyphs);
		font->Read(font, &glyphsSize, glyphs);
	}

	PSF1_FONT *finalFont;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF1_FONT), (void**)&finalFont);
	finalFont->header = header;
	finalFont->glyphs = glyphs;
	return finalFont;
}

int memcmp(const void* aptr, const void* bptr, size_t n){
	const unsigned char* a = aptr, *b = bptr;
	for (size_t i = 0; i < n; i++){
		if (a[i] < b[i]) return -1;
		else if (a[i] > b[i]) return 1;
	}
	return 0;
}

typedef struct {
	FRAMEBUFFER *framebuffer;
	PSF1_FONT *font;
	EFI_MEMORY_DESCRIPTOR *map;
	UINTN mapSize;
	UINTN mapDescriptorSize;
} BOOT_INFO;

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
	InitializeLib(ImageHandle, SystemTable);

	EFI_FILE* Kernel = LoadFile(NULL, L"kernel.elf", ImageHandle, SystemTable);
	if (Kernel == NULL) Print(L"Unable to load kernel.\n\r");
	else Print(L"Kernel loaded successfully.\n\r");

	Elf64_Ehdr header;
	{
		UINTN FileInfoSize;
		EFI_FILE_INFO* FileInfo;
		Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, NULL);
		SystemTable->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo);
		Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, (void**)&FileInfo);

		UINTN size = sizeof(header);
		Kernel->Read(Kernel, &size, &header);
	}

	if (
		memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
		header.e_ident[EI_CLASS] != ELFCLASS64 ||
		header.e_ident[EI_DATA] != ELFDATA2LSB ||
		header.e_type != ET_EXEC ||
		header.e_machine != EM_X86_64 ||
		header.e_version != EV_CURRENT
	) Print(L"Bad kernel format.\r\n");
	else Print(L"Kernel header checked successfully.\r\n");

	Elf64_Phdr* phdrs;
	{
		Kernel->SetPosition(Kernel, header.e_phoff);
		UINTN size = header.e_phnum * header.e_phentsize;
		SystemTable->BootServices->AllocatePool(EfiLoaderData, size, (void**)&phdrs);
		Kernel->Read(Kernel, &size, phdrs);
	}

	for (
		Elf64_Phdr* phdr = phdrs;
		(char*)phdr < (char*)phdrs + header.e_phnum * header.e_phentsize;
		phdr = (Elf64_Phdr*)((char*)phdr + header.e_phentsize)
	)
		switch (phdr->p_type){
			case PT_LOAD:
			{
				int pages = (phdr->p_memsz + 0x1000 - 1) / 0x1000;
				Elf64_Addr segment = phdr->p_paddr;
				SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

				Kernel->SetPosition(Kernel, phdr->p_offset);
				UINTN size = phdr->p_filesz;
				Kernel->Read(Kernel, &size, (void*)segment);
				break;
			}
		}

	Print(L"Kernel loaded.\n\r");
	
	PSF1_FONT *newFont = LoadPSF1Font(NULL, L"zap-light16.psf", ImageHandle, SystemTable);
	if(newFont) Print(L"Font loaded. Size = %d\n\r", newFont->header->size);
	else Print(L"Invalid font or not found.\n\r");
	
	FRAMEBUFFER *newBuffer = InitializeGOP();

	Print(L"Base: 0x%x\n\rSize: 0x%x\n\rWidth: %d\n\rHeight: %d\n\rPixelsPerScanline: %d\n\r",
	newBuffer->address,
	newBuffer->size,
	newBuffer->width,
	newBuffer->height,
	newBuffer->pixelsPerScanline);

	EFI_MEMORY_DESCRIPTOR *map = NULL;
	UINTN mapSize, mapKey;
	UINTN descriptorSize;
	UINT32 descriptorVersion;
	{
		SystemTable->BootServices->GetMemoryMap(&mapSize, map, &mapKey, &descriptorSize, &descriptorVersion);
		SystemTable->BootServices->AllocatePool(EfiLoaderData, mapSize, (void**)&map);
		SystemTable->BootServices->GetMemoryMap(&mapSize, map, &mapKey, &descriptorSize, &descriptorVersion);	
	}

	void (*KernelStart)(BOOT_INFO*) = ((__attribute__((sysv_abi)) void (*)(BOOT_INFO*) ) header.e_entry);

	BOOT_INFO bootInfo;
	bootInfo.framebuffer = newBuffer;
	bootInfo.font = newFont;
	bootInfo.map = map;
	bootInfo.mapSize = mapSize;
	bootInfo.mapDescriptorSize = descriptorSize;

	SystemTable->BootServices->ExitBootServices(ImageHandle, mapKey);

	KernelStart(&bootInfo);

	return EFI_SUCCESS; // Exit the UEFI application
}
