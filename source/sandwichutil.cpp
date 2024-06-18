#include <boss.hpp>
#include "sandwichutil.hpp"

static const uint64 gCrcTable[256] = {
    Unsigned64(0x0000000000000000), Unsigned64(0x42f0e1eba9ea3693),
    Unsigned64(0x85e1c3d753d46d26), Unsigned64(0xc711223cfa3e5bb5),
    Unsigned64(0x493366450e42ecdf), Unsigned64(0x0bc387aea7a8da4c),
    Unsigned64(0xccd2a5925d9681f9), Unsigned64(0x8e224479f47cb76a),
    Unsigned64(0x9266cc8a1c85d9be), Unsigned64(0xd0962d61b56fef2d),
    Unsigned64(0x17870f5d4f51b498), Unsigned64(0x5577eeb6e6bb820b),
    Unsigned64(0xdb55aacf12c73561), Unsigned64(0x99a54b24bb2d03f2),
    Unsigned64(0x5eb4691841135847), Unsigned64(0x1c4488f3e8f96ed4),
    Unsigned64(0x663d78ff90e185ef), Unsigned64(0x24cd9914390bb37c),
    Unsigned64(0xe3dcbb28c335e8c9), Unsigned64(0xa12c5ac36adfde5a),
    Unsigned64(0x2f0e1eba9ea36930), Unsigned64(0x6dfeff5137495fa3),
    Unsigned64(0xaaefdd6dcd770416), Unsigned64(0xe81f3c86649d3285),
    Unsigned64(0xf45bb4758c645c51), Unsigned64(0xb6ab559e258e6ac2),
    Unsigned64(0x71ba77a2dfb03177), Unsigned64(0x334a9649765a07e4),
    Unsigned64(0xbd68d2308226b08e), Unsigned64(0xff9833db2bcc861d),
    Unsigned64(0x388911e7d1f2dda8), Unsigned64(0x7a79f00c7818eb3b),
    Unsigned64(0xcc7af1ff21c30bde), Unsigned64(0x8e8a101488293d4d),
    Unsigned64(0x499b3228721766f8), Unsigned64(0x0b6bd3c3dbfd506b),
    Unsigned64(0x854997ba2f81e701), Unsigned64(0xc7b97651866bd192),
    Unsigned64(0x00a8546d7c558a27), Unsigned64(0x4258b586d5bfbcb4),
    Unsigned64(0x5e1c3d753d46d260), Unsigned64(0x1cecdc9e94ace4f3),
    Unsigned64(0xdbfdfea26e92bf46), Unsigned64(0x990d1f49c77889d5),
    Unsigned64(0x172f5b3033043ebf), Unsigned64(0x55dfbadb9aee082c),
    Unsigned64(0x92ce98e760d05399), Unsigned64(0xd03e790cc93a650a),
    Unsigned64(0xaa478900b1228e31), Unsigned64(0xe8b768eb18c8b8a2),
    Unsigned64(0x2fa64ad7e2f6e317), Unsigned64(0x6d56ab3c4b1cd584),
    Unsigned64(0xe374ef45bf6062ee), Unsigned64(0xa1840eae168a547d),
    Unsigned64(0x66952c92ecb40fc8), Unsigned64(0x2465cd79455e395b),
    Unsigned64(0x3821458aada7578f), Unsigned64(0x7ad1a461044d611c),
    Unsigned64(0xbdc0865dfe733aa9), Unsigned64(0xff3067b657990c3a),
    Unsigned64(0x711223cfa3e5bb50), Unsigned64(0x33e2c2240a0f8dc3),
    Unsigned64(0xf4f3e018f031d676), Unsigned64(0xb60301f359dbe0e5),
    Unsigned64(0xda050215ea6c212f), Unsigned64(0x98f5e3fe438617bc),
    Unsigned64(0x5fe4c1c2b9b84c09), Unsigned64(0x1d14202910527a9a),
    Unsigned64(0x93366450e42ecdf0), Unsigned64(0xd1c685bb4dc4fb63),
    Unsigned64(0x16d7a787b7faa0d6), Unsigned64(0x5427466c1e109645),
    Unsigned64(0x4863ce9ff6e9f891), Unsigned64(0x0a932f745f03ce02),
    Unsigned64(0xcd820d48a53d95b7), Unsigned64(0x8f72eca30cd7a324),
    Unsigned64(0x0150a8daf8ab144e), Unsigned64(0x43a04931514122dd),
    Unsigned64(0x84b16b0dab7f7968), Unsigned64(0xc6418ae602954ffb),
    Unsigned64(0xbc387aea7a8da4c0), Unsigned64(0xfec89b01d3679253),
    Unsigned64(0x39d9b93d2959c9e6), Unsigned64(0x7b2958d680b3ff75),
    Unsigned64(0xf50b1caf74cf481f), Unsigned64(0xb7fbfd44dd257e8c),
    Unsigned64(0x70eadf78271b2539), Unsigned64(0x321a3e938ef113aa),
    Unsigned64(0x2e5eb66066087d7e), Unsigned64(0x6cae578bcfe24bed),
    Unsigned64(0xabbf75b735dc1058), Unsigned64(0xe94f945c9c3626cb),
    Unsigned64(0x676dd025684a91a1), Unsigned64(0x259d31cec1a0a732),
    Unsigned64(0xe28c13f23b9efc87), Unsigned64(0xa07cf2199274ca14),
    Unsigned64(0x167ff3eacbaf2af1), Unsigned64(0x548f120162451c62),
    Unsigned64(0x939e303d987b47d7), Unsigned64(0xd16ed1d631917144),
    Unsigned64(0x5f4c95afc5edc62e), Unsigned64(0x1dbc74446c07f0bd),
    Unsigned64(0xdaad56789639ab08), Unsigned64(0x985db7933fd39d9b),
    Unsigned64(0x84193f60d72af34f), Unsigned64(0xc6e9de8b7ec0c5dc),
    Unsigned64(0x01f8fcb784fe9e69), Unsigned64(0x43081d5c2d14a8fa),
    Unsigned64(0xcd2a5925d9681f90), Unsigned64(0x8fdab8ce70822903),
    Unsigned64(0x48cb9af28abc72b6), Unsigned64(0x0a3b7b1923564425),
    Unsigned64(0x70428b155b4eaf1e), Unsigned64(0x32b26afef2a4998d),
    Unsigned64(0xf5a348c2089ac238), Unsigned64(0xb753a929a170f4ab),
    Unsigned64(0x3971ed50550c43c1), Unsigned64(0x7b810cbbfce67552),
    Unsigned64(0xbc902e8706d82ee7), Unsigned64(0xfe60cf6caf321874),
    Unsigned64(0xe224479f47cb76a0), Unsigned64(0xa0d4a674ee214033),
    Unsigned64(0x67c58448141f1b86), Unsigned64(0x253565a3bdf52d15),
    Unsigned64(0xab1721da49899a7f), Unsigned64(0xe9e7c031e063acec),
    Unsigned64(0x2ef6e20d1a5df759), Unsigned64(0x6c0603e6b3b7c1ca),
    Unsigned64(0xf6fae5c07d3274cd), Unsigned64(0xb40a042bd4d8425e),
    Unsigned64(0x731b26172ee619eb), Unsigned64(0x31ebc7fc870c2f78),
    Unsigned64(0xbfc9838573709812), Unsigned64(0xfd39626eda9aae81),
    Unsigned64(0x3a28405220a4f534), Unsigned64(0x78d8a1b9894ec3a7),
    Unsigned64(0x649c294a61b7ad73), Unsigned64(0x266cc8a1c85d9be0),
    Unsigned64(0xe17dea9d3263c055), Unsigned64(0xa38d0b769b89f6c6),
    Unsigned64(0x2daf4f0f6ff541ac), Unsigned64(0x6f5faee4c61f773f),
    Unsigned64(0xa84e8cd83c212c8a), Unsigned64(0xeabe6d3395cb1a19),
    Unsigned64(0x90c79d3fedd3f122), Unsigned64(0xd2377cd44439c7b1),
    Unsigned64(0x15265ee8be079c04), Unsigned64(0x57d6bf0317edaa97),
    Unsigned64(0xd9f4fb7ae3911dfd), Unsigned64(0x9b041a914a7b2b6e),
    Unsigned64(0x5c1538adb04570db), Unsigned64(0x1ee5d94619af4648),
    Unsigned64(0x02a151b5f156289c), Unsigned64(0x4051b05e58bc1e0f),
    Unsigned64(0x87409262a28245ba), Unsigned64(0xc5b073890b687329),
    Unsigned64(0x4b9237f0ff14c443), Unsigned64(0x0962d61b56fef2d0),
    Unsigned64(0xce73f427acc0a965), Unsigned64(0x8c8315cc052a9ff6),
    Unsigned64(0x3a80143f5cf17f13), Unsigned64(0x7870f5d4f51b4980),
    Unsigned64(0xbf61d7e80f251235), Unsigned64(0xfd913603a6cf24a6),
    Unsigned64(0x73b3727a52b393cc), Unsigned64(0x31439391fb59a55f),
    Unsigned64(0xf652b1ad0167feea), Unsigned64(0xb4a25046a88dc879),
    Unsigned64(0xa8e6d8b54074a6ad), Unsigned64(0xea16395ee99e903e),
    Unsigned64(0x2d071b6213a0cb8b), Unsigned64(0x6ff7fa89ba4afd18),
    Unsigned64(0xe1d5bef04e364a72), Unsigned64(0xa3255f1be7dc7ce1),
    Unsigned64(0x64347d271de22754), Unsigned64(0x26c49cccb40811c7),
    Unsigned64(0x5cbd6cc0cc10fafc), Unsigned64(0x1e4d8d2b65facc6f),
    Unsigned64(0xd95caf179fc497da), Unsigned64(0x9bac4efc362ea149),
    Unsigned64(0x158e0a85c2521623), Unsigned64(0x577eeb6e6bb820b0),
    Unsigned64(0x906fc95291867b05), Unsigned64(0xd29f28b9386c4d96),
    Unsigned64(0xcedba04ad0952342), Unsigned64(0x8c2b41a1797f15d1),
    Unsigned64(0x4b3a639d83414e64), Unsigned64(0x09ca82762aab78f7),
    Unsigned64(0x87e8c60fded7cf9d), Unsigned64(0xc51827e4773df90e),
    Unsigned64(0x020905d88d03a2bb), Unsigned64(0x40f9e43324e99428),
    Unsigned64(0x2cffe7d5975e55e2), Unsigned64(0x6e0f063e3eb46371),
    Unsigned64(0xa91e2402c48a38c4), Unsigned64(0xebeec5e96d600e57),
    Unsigned64(0x65cc8190991cb93d), Unsigned64(0x273c607b30f68fae),
    Unsigned64(0xe02d4247cac8d41b), Unsigned64(0xa2dda3ac6322e288),
    Unsigned64(0xbe992b5f8bdb8c5c), Unsigned64(0xfc69cab42231bacf),
    Unsigned64(0x3b78e888d80fe17a), Unsigned64(0x7988096371e5d7e9),
    Unsigned64(0xf7aa4d1a85996083), Unsigned64(0xb55aacf12c735610),
    Unsigned64(0x724b8ecdd64d0da5), Unsigned64(0x30bb6f267fa73b36),
    Unsigned64(0x4ac29f2a07bfd00d), Unsigned64(0x08327ec1ae55e69e),
    Unsigned64(0xcf235cfd546bbd2b), Unsigned64(0x8dd3bd16fd818bb8),
    Unsigned64(0x03f1f96f09fd3cd2), Unsigned64(0x41011884a0170a41),
    Unsigned64(0x86103ab85a2951f4), Unsigned64(0xc4e0db53f3c36767),
    Unsigned64(0xd8a453a01b3a09b3), Unsigned64(0x9a54b24bb2d03f20),
    Unsigned64(0x5d45907748ee6495), Unsigned64(0x1fb5719ce1045206),
    Unsigned64(0x919735e51578e56c), Unsigned64(0xd367d40ebc92d3ff),
    Unsigned64(0x1476f63246ac884a), Unsigned64(0x568617d9ef46bed9),
    Unsigned64(0xe085162ab69d5e3c), Unsigned64(0xa275f7c11f7768af),
    Unsigned64(0x6564d5fde549331a), Unsigned64(0x279434164ca30589),
    Unsigned64(0xa9b6706fb8dfb2e3), Unsigned64(0xeb46918411358470),
    Unsigned64(0x2c57b3b8eb0bdfc5), Unsigned64(0x6ea7525342e1e956),
    Unsigned64(0x72e3daa0aa188782), Unsigned64(0x30133b4b03f2b111),
    Unsigned64(0xf7021977f9cceaa4), Unsigned64(0xb5f2f89c5026dc37),
    Unsigned64(0x3bd0bce5a45a6b5d), Unsigned64(0x79205d0e0db05dce),
    Unsigned64(0xbe317f32f78e067b), Unsigned64(0xfcc19ed95e6430e8),
    Unsigned64(0x86b86ed5267cdbd3), Unsigned64(0xc4488f3e8f96ed40),
    Unsigned64(0x0359ad0275a8b6f5), Unsigned64(0x41a94ce9dc428066),
    Unsigned64(0xcf8b0890283e370c), Unsigned64(0x8d7be97b81d4019f),
    Unsigned64(0x4a6acb477bea5a2a), Unsigned64(0x089a2aacd2006cb9),
    Unsigned64(0x14dea25f3af9026d), Unsigned64(0x562e43b4931334fe),
    Unsigned64(0x913f6188692d6f4b), Unsigned64(0xd3cf8063c0c759d8),
    Unsigned64(0x5dedc41a34bbeeb2), Unsigned64(0x1f1d25f19d51d821),
    Unsigned64(0xd80c07cd676f8394), Unsigned64(0x9afce626ce85b507)};

String SandWichUtil::ToCRC64(bytes binary, sint32 length)
{
    uint64 CrcCode = oxFFFFFFFFFFFFFFFF;
    bytes CrcFocus = binary;
    for(int i = 0; i < length; ++i)
        CrcCode = gCrcTable[((CrcCode >> 56) & 0xFF) ^ *(CrcFocus++)] ^ (CrcCode << 8);
    return String::Format("%016x", CrcCode);
}

static const char gBase64Encode[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'};

String SandWichUtil::ToBASE64(bytes binary, sint32 length)
{
    unsigned char input[3] = {0, 0, 0};
    unsigned char output[4] = {0, 0, 0, 0};
    int index, i, j, size = (4 * (length / 3)) + ((length % 3)? 4 : 0) + 1;
    bytes p, plen = binary + length - 1;

    chararray Result;
    char* encoded = Result.AtDumpingAdded(size);
    for(i = 0, j = 0, p = binary; p <= plen; i++, p++)
    {
        index = i % 3;
        input[index] = *p;
        if(index == 2 || p == plen)
        {
            output[0] = ((input[0] & 0xFC) >> 2);
            output[1] = ((input[0] & 0x03) << 4) | ((input[1] & 0xF0) >> 4);
            output[2] = ((input[1] & 0x0F) << 2) | ((input[2] & 0xC0) >> 6);
            output[3] = (input[2] & 0x3F);
            encoded[j++] = gBase64Encode[output[0]];
            encoded[j++] = gBase64Encode[output[1]];
            encoded[j++] = (index == 0)? '=' : gBase64Encode[output[2]];
            encoded[j++] = (index < 2)? '=' : gBase64Encode[output[3]];
            input[0] = input[1] = input[2] = 0;
        }
    }
    encoded[j] = '\0';
    return String(ToReference(Result));
}

static int gBase64Decode[] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

buffer SandWichUtil::FromBASE64(chars base64, sint32 length)
{
    chars cp;
    int space_idx = 0, phase = 0;
    int d, prev_d = 0;
    unsigned char c;

    buffer Result = Buffer::Alloc(BOSS_DBG length * 6 / 8);
    unsigned char* decoded = (unsigned char*) Result;
    for(cp = base64; *cp != '\0'; ++cp)
    {
        d = gBase64Decode[*cp & 0xFF];
        if(d != -1)
        {
            switch(phase)
            {
            case 0: ++phase; break;
            case 1:
                c = ((prev_d << 2) | ((d & 0x30) >> 4));
                decoded[space_idx++] = c;
                ++phase;
                break;
            case 2:
                c = (((prev_d & 0x0F) << 4) | ((d & 0x3C) >> 2));
                decoded[space_idx++] = c;
                ++phase;
                break;
            case 3:
                c = (((prev_d & 0x03) << 6) | d);
                decoded[space_idx++] = c;
                phase = 0;
                break;
            }
            prev_d = d;
        }
    }
    return Result;
}
