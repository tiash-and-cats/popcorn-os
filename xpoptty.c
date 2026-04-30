#include <efi.h>
#include <efilib.h>

#define popt_CUR_DASH        0
#define popt_CUR_BLOCK       1

typedef struct {
    CHAR16 ch;
    bool bold;
    bool underline;
    int fgColor;
    int bgColor;
} popt_TTYCell;

typedef struct {
    int x, y;
    int style;
} popt_TTYCursor;

typedef struct {
    UINTN width;
    UINTN height;
    popt_TTYCell **cells;
    popt_TTYCursor cur;
    void (*render)(struct popt_TTY *tty);
    void *extra;
} popt_TTY;

popt_TTY *popt_create_uefi(EFI_SYSTEM_TABLE *SystemTable) {
    popt_TTY *tty = popk_memalloc(sizeof(popt_TTY));
    UINTN Columns, Rows;

    // Get current mode's width and height
    SystemTable->ConOut->QueryMode(
        SystemTable->ConOut,
        SystemTable->ConOut->Mode->Mode,
        &Columns,
        &Rows
    );

    tty->width = Columns;
    tty->height = Rows;
    tty->extra = (void*)SystemTable->ConOut;

    // Allocate 2D grid
    tty->cells = popk_memalloc(sizeof(popt_TTYCell*) * tty->height);
    for (UINTN i = 0; i < tty->height; i++) {
        tty->cells[i] = popk_memalloc(sizeof(popt_TTYCell) * tty->width);
        for (UINTN j = 0; j < tty->width; j++) {
            tty->cells[i][j].ch = L' ';
            tty->cells[i][j].bold = false;
            tty->cells[i][j].underline = false;
            tty->cells[i][j].fgColor = EFI_LIGHTGRAY;
            tty->cells[i][j].bgColor = EFI_BLACK;
        }
    }

    tty->render = popt_render_uefi;
    return tty;
}

void popt_print(popt_TTY *tty, const CHAR16 *str) {
    int cidx = tty->cur.y * tty->width + tty->cur.x;
    int len = 0;
    while (str[len]) len++;

    // Scroll if needed
    if (cidx + len > (int)(tty->width * tty->height)) {
        int rows = len / tty->width + 1;
        for (UINTN i = rows; i < tty->height; i++) {
            for (UINTN j = 0; j < tty->width; j++) {
                tty->cells[i - rows][j] = tty->cells[i][j];
            }
        }
        // Clear bottom rows
        for (UINTN i = tty->height - rows; i < tty->height; i++) {
            for (UINTN j = 0; j < tty->width; j++) {
                tty->cells[i][j].ch = L' ';
                tty->cells[i][j].fgColor = EFI_LIGHTGRAY;
                tty->cells[i][j].bgColor = EFI_BLACK;
            }
        }
        cidx = (tty->height - rows) * tty->width; // reset cursor
    }

    // Write characters
    for (int i = 0; i < len; i++) {
        int pos = cidx + i;
        int row = pos / tty->width;
        int col = pos % tty->width;
        tty->cells[row][col].ch = str[i];
    }

    // Advance cursor
    tty->cur.x = (cidx + len) % tty->width;
    tty->cur.y = (cidx + len) / tty->width;
}

void popt_render_uefi(popt_TTY *tty) {
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *conout = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*)tty->extra;
    for (UINTN y = 0; y < tty->height; y++) {
        for (UINTN x = 0; x < tty->width; x++) {
            popt_TTYCell cell = tty->cells[y][x];
            UINTN style = EFI_TEXT_ATTR(cell.fgColor, cell.bgColor);

            // Set color attribute before printing
            conout->SetAttribute(conout, style);

            // Move cursor to (x,y)
            conout->SetCursorPosition(conout, x, y);

            // Print character
            CHAR16 buf[2] = { cell.ch, L'\0' };
            conout->OutputString(conout, buf);
        }
    }
}
