//
// Created by troplo on 10/19/26.
//

#ifndef NOMADARCHIVES_HASHINGUTILS_H
#define NOMADARCHIVES_HASHINGUTILS_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Nomad::Archives {
    class HashingUtils {
      public:
        static constexpr uint64_t CRCtable64[256] = {
            0x0000000000000000ULL, 0x42F0E1EBA9EA3693ULL, 0xF6FAE5C07D3274CDULL,
            0xB40A042BD4D8425EULL, 0x85E1C3D753D46D26ULL, 0xC711223CFA3E5BB5ULL,
            0x731B26172EE619EBULL, 0x31EBC7FC870C2F78ULL, 0x493366450E42ECDFULL,
            0x0BC387AEA7A8DA4CULL, 0xBFC9838573709812ULL, 0xFD39626EDA9AAE81ULL,
            0xCCD2A5925D9681F9ULL, 0x8E224479F47CB76AULL, 0x3A28405220A4F534ULL,
            0x78D8A1B9894EC3A7ULL, 0x9266CC8A1C85D9BEULL, 0xD0962D61B56FEF2DULL,
            0x649C294A61B7AD73ULL, 0x266CC8A1C85D9BE0ULL, 0x17870F5D4F51B498ULL,
            0x5577EEB6E6BB820BULL, 0xE17DEA9D3263C055ULL, 0xA38D0B769B89F6C6ULL,
            0xDB55AACF12C73561ULL, 0x99A54B24BB2D03F2ULL, 0x2DAF4F0F6FF541ACULL,
            0x6F5FAEE4C61F773FULL, 0x5EB4691841135847ULL, 0x1C4488F3E8F96ED4ULL,
            0xA84E8CD83C212C8AULL, 0xEABE6D3395CB1A19ULL, 0x663D78FF90E185EFULL,
            0x24CD9914390BB37CULL, 0x90C79D3FEDD3F122ULL, 0xD2377CD44439C7B1ULL,
            0xE3DCBB28C335E8C9ULL, 0xA12C5AC36ADFDE5AULL, 0x15265EE8BE079C04ULL,
            0x57D6BF0317EDAA97ULL, 0x2F0E1EBA9EA36930ULL, 0x6DFEFF5137495FA3ULL,
            0xD9F4FB7AE3911DFDULL, 0x9B041A914A7B2B6EULL, 0xAAEFDD6DCD770416ULL,
            0xE81F3C86649D3285ULL, 0x5C1538ADB04570DBULL, 0x1EE5D94619AF4648ULL,
            0xF45BB4758C645C51ULL, 0xB6AB559E258E6AC2ULL, 0x02A151B5F156289CULL,
            0x4051B05E58BC1E0FULL, 0x71BA77A2DFB03177ULL, 0x334A9649765A07E4ULL,
            0x87409262A28245BAULL, 0xC5B073890B687329ULL, 0xBD68D2308226B08EULL,
            0xFF9833DB2BCC861DULL, 0x4B9237F0FF14C443ULL, 0x0962D61B56FEF2D0ULL,
            0x388911E7D1F2DDA8ULL, 0x7A79F00C7818EB3BULL, 0xCE73F427ACC0A965ULL,
            0x8C8315CC052A9FF6ULL, 0xCC7AF1FF21C30BDEULL, 0x8E8A101488293D4DULL,
            0x3A80143F5CF17F13ULL, 0x7870F5D4F51B4980ULL, 0x499B3228721766F8ULL,
            0x0B6BD3C3DBFD506BULL, 0xBF61D7E80F251235ULL, 0xFD913603A6CF24A6ULL,
            0x854997BA2F81E701ULL, 0xC7B97651866BD192ULL, 0x73B3727A52B393CCULL,
            0x31439391FB59A55FULL, 0x00A8546D7C558A27ULL, 0x4258B586D5BFBCB4ULL,
            0xF652B1AD0167FEEAULL, 0xB4A25046A88DC879ULL, 0x5E1C3D753D46D260ULL,
            0x1CECDC9E94ACE4F3ULL, 0xA8E6D8B54074A6ADULL, 0xEA16395EE99E903EULL,
            0xDBFDFEA26E92BF46ULL, 0x990D1F49C77889D5ULL, 0x2D071B6213A0CB8BULL,
            0x6FF7FA89BA4AFD18ULL, 0x172F5B3033043EBFULL, 0x55DFBADB9AEE082CULL,
            0xE1D5BEF04E364A72ULL, 0xA3255F1BE7DC7CE1ULL, 0x92CE98E760D05399ULL,
            0xD03E790CC93A650AULL, 0x64347D271DE22754ULL, 0x26C49CCCB40811C7ULL,
            0xAA478900B1228E31ULL, 0xE8B768EB18C8B8A2ULL, 0x5CBD6CC0CC10FAFCULL,
            0x1E4D8D2B65FACC6FULL, 0x2FA64AD7E2F6E317ULL, 0x6D56AB3C4B1CD584ULL,
            0xD95CAF179FC497DAULL, 0x9BAC4EFC362EA149ULL, 0xE374EF45BF6062EEULL,
            0xA1840EAE168A547DULL, 0x158E0A85C2521623ULL, 0x577EEB6E6BB820B0ULL,
            0x66952C92ECB40FC8ULL, 0x2465CD79455E395BULL, 0x906FC95291867B05ULL,
            0xD29F28B9386C4D96ULL, 0x3821458AADA7578FULL, 0x7AD1A461044D611CULL,
            0xCEDBA04AD0952342ULL, 0x8C2B41A1797F15D1ULL, 0xBDC0865DFE733AA9ULL,
            0xFF3067B657990C3AULL, 0x4B3A639D83414E64ULL, 0x09CA82762AAB78F7ULL,
            0x711223CFA3E5BB50ULL, 0x33E2C2240A0F8DC3ULL, 0x87E8C60FDED7CF9DULL,
            0xC51827E4773DF90EULL, 0xF4F3E018F031D676ULL, 0xB60301F359DBE0E5ULL,
            0x020905D88D03A2BBULL, 0x40F9E43324E99428ULL, 0xDA050215EA6C212FULL,
            0x98F5E3FE438617BCULL, 0x2CFFE7D5975E55E2ULL, 0x6E0F063E3EB46371ULL,
            0x5FE4C1C2B9B84C09ULL, 0x1D14202910527A9AULL, 0xA91E2402C48A38C4ULL,
            0xEBEEC5E96D600E57ULL, 0x93366450E42ECDF0ULL, 0xD1C685BB4DC4FB63ULL,
            0x65CC8190991CB93DULL, 0x273C607B30F68FAEULL, 0x16D7A787B7FAA0D6ULL,
            0x5427466C1E109645ULL, 0xE02D4247CAC8D41BULL, 0xA2DDA3AC6322E288ULL,
            0x4863CE9FF6E9F891ULL, 0x0A932F745F03CE02ULL, 0xBE992B5F8BDB8C5CULL,
            0xFC69CAB42231BACFULL, 0xCD820D48A53D95B7ULL, 0x8F72ECA30CD7A324ULL,
            0x3B78E888D80FE17AULL, 0x7988096371E5D7E9ULL, 0x0150A8DAF8AB144EULL,
            0x43A04931514122DDULL, 0xF7AA4D1A85996083ULL, 0xB55AACF12C735610ULL,
            0x84B16B0DAB7F7968ULL, 0xC6418AE602954FFBULL, 0x724B8ECDD64D0DA5ULL,
            0x30BB6F267FA73B36ULL, 0xBC387AEA7A8DA4C0ULL, 0xFEC89B01D3679253ULL,
            0x4AC29F2A07BFD00DULL, 0x08327EC1AE55E69EULL, 0x39D9B93D2959C9E6ULL,
            0x7B2958D680B3FF75ULL, 0xCF235CFD546BBD2BULL, 0x8DD3BD16FD818BB8ULL,
            0xF50B1CAF74CF481FULL, 0xB7FBFD44DD257E8CULL, 0x03F1F96F09FD3CD2ULL,
            0x41011884A0170A41ULL, 0x70EADF78271B2539ULL, 0x321A3E938EF113AAULL,
            0x86103AB85A2951F4ULL, 0xC4E0DB53F3C36767ULL, 0x2E5EB66066087D7EULL,
            0x6CAE578BCFE24BEDULL, 0xD8A453A01B3A09B3ULL, 0x9A54B24BB2D03F20ULL,
            0xABBF75B735DC1058ULL, 0xE94F945C9C3626CBULL, 0x5D45907748EE6495ULL,
            0x1FB5719CE1045206ULL, 0x676DD025684A91A1ULL, 0x259D31CEC1A0A732ULL,
            0x919735E51578E56CULL, 0xD367D40EBC92D3FFULL, 0xE28C13F23B9EFC87ULL,
            0xA07CF2199274CA14ULL, 0x1476F63246AC884AULL, 0x568617D9EF46BED9ULL,
            0x167FF3EACBAF2AF1ULL, 0x548F120162451C62ULL, 0xE085162AB69D5E3CULL,
            0xA275F7C11F7768AFULL, 0x939E303D987B47D7ULL, 0xD16ED1D631917144ULL,
            0x6564D5FDE549331AULL, 0x279434164CA30589ULL, 0x5F4C95AFC5EDC62EULL,
            0x1DBC74446C07F0BDULL, 0xA9B6706FB8DFB2E3ULL, 0xEB46918411358470ULL,
            0xDAAD56789639AB08ULL, 0x985DB7933FD39D9BULL, 0x2C57B3B8EB0BDFC5ULL,
            0x6EA7525342E1E956ULL, 0x84193F60D72AF34FULL, 0xC6E9DE8B7EC0C5DCULL,
            0x72E3DAA0AA188782ULL, 0x30133B4B03F2B111ULL, 0x01F8FCB784FE9E69ULL,
            0x43081D5C2D14A8FAULL, 0xF7021977F9CCEAA4ULL, 0xB5F2F89C5026DC37ULL,
            0xCD2A5925D9681F90ULL, 0x8FDAB8CE70822903ULL, 0x3BD0BCE5A45A6B5DULL,
            0x79205D0E0DB05DCEULL, 0x48CB9AF28ABC72B6ULL, 0x0A3B7B1923564425ULL,
            0xBE317F32F78E067BULL, 0xFCC19ED95E6430E8ULL, 0x70428B155B4EAF1EULL,
            0x32B26AFEF2A4998DULL, 0x86B86ED5267CDBD3ULL, 0xC4488F3E8F96ED40ULL,
            0xF5A348C2089AC238ULL, 0xB753A929A170F4ABULL, 0x0359AD0275A8B6F5ULL,
            0x41A94CE9DC428066ULL, 0x3971ED50550C43C1ULL, 0x7B810CBBFCE67552ULL,
            0xCF8B0890283E370CULL, 0x8D7BE97B81D4019FULL, 0xBC902E8706D82EE7ULL,
            0xFE60CF6CAF321874ULL, 0x4A6ACB477BEA5A2AULL, 0x089A2AACD2006CB9ULL,
            0xE224479F47CB76A0ULL, 0xA0D4A674EE214033ULL, 0x14DEA25F3AF9026DULL,
            0x562E43B4931334FEULL, 0x67C58448141F1B86ULL, 0x253565A3BDF52D15ULL,
            0x913F6188692D6F4BULL, 0xD3CF8063C0C759D8ULL, 0xAB1721DA49899A7FULL,
            0xE9E7C031E063ACECULL, 0x5DEDC41A34BBEEB2ULL, 0x1F1D25F19D51D821ULL,
            0x2EF6E20D1A5DF759ULL, 0x6C0603E6B3B7C1CAULL, 0xD80C07CD676F8394ULL,
            0x9AFCE626CE85B507ULL};

        static constexpr uint32_t CRCtable32[256] = {
            0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u,
            0x706AF48Fu, 0xE963A535u, 0x9E6495A3u, 0x0EDB8832u, 0x79DCB8A4u,
            0xE0D5E91Eu, 0x97D2D988u, 0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u,
            0x90BF1D91u, 0x1DB71064u, 0x6AB020F2u, 0xF3B97148u, 0x84BE41DEu,
            0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u, 0x136C9856u,
            0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u,
            0xFA0F3D63u, 0x8D080DF5u, 0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u,
            0xA2677172u, 0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu,
            0x35B5A8FAu, 0x42B2986Cu, 0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u,
            0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u, 0x26D930ACu, 0x51DE003Au,
            0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u, 0xCFBA9599u,
            0xB8BDA50Fu, 0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u,
            0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du, 0x76DC4190u,
            0x01DB7106u, 0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu,
            0x9FBFE4A5u, 0xE8B8D433u, 0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu,
            0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du, 0x91646C97u, 0xE6635C01u,
            0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu, 0x6C0695EDu,
            0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u, 0x65B0D9C6u, 0x12B7E950u,
            0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u,
            0xFBD44C65u, 0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u,
            0x4ADFA541u, 0x3DD895D7u, 0xA4D1C46Du, 0xD3D6F4FBu, 0x4369E96Au,
            0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u, 0x44042D73u, 0x33031DE5u,
            0xAA0A4C5Fu, 0xDD0D7CC9u, 0x5005713Cu, 0x270241AAu, 0xBE0B1010u,
            0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu,
            0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u,
            0x2EB40D81u, 0xB7BD5C3Bu, 0xC0BA6CADu, 0xEDB88320u, 0x9ABFB3B6u,
            0x03B6E20Cu, 0x74B1D29Au, 0xEAD54739u, 0x9DD277AFu, 0x04DB2615u,
            0x73DC1683u, 0xE3630B12u, 0x94643B84u, 0x0D6D6A3Eu, 0x7A6A5AA8u,
            0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u, 0xF00F9344u,
            0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu,
            0x196C3671u, 0x6E6B06E7u, 0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au,
            0x67DD4ACCu, 0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u,
            0xD6D6A3E8u, 0xA1D1937Eu, 0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u,
            0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu, 0xD80D2BDAu, 0xAF0A1B4Cu,
            0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u, 0x316E8EEFu,
            0x4669BE79u, 0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u,
            0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu, 0xC5BA3BBEu,
            0xB2BD0B28u, 0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u,
            0x2CD99E8Bu, 0x5BDEAE1Du, 0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu,
            0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu, 0x72076785u, 0x05005713u,
            0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u, 0x92D28E9Bu,
            0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u, 0x86D3D2D4u, 0xF1D4E242u,
            0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u,
            0x18B74777u, 0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu,
            0x8F659EFFu, 0xF862AE69u, 0x616BFFD3u, 0x166CCF45u, 0xA00AE278u,
            0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u, 0xA7672661u, 0xD06016F7u,
            0x4969474Du, 0x3E6E77DBu, 0xAED16A4Au, 0xD9D65ADCu, 0x40DF0B66u,
            0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
            0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u,
            0xCDD70693u, 0x54DE5729u, 0x23D967BFu, 0xB3667A2Eu, 0xC4614AB8u,
            0x5D681B02u, 0x2A6F2B94u, 0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu,
            0x2D02EF8Du};

        // CRC-64 (ECMA-182)
        static constexpr uint64_t CreateCRC64(const char *data,
                                              size_t length) noexcept {
            uint64_t result = 0;
            for (size_t i = 0; i < length; ++i) {
                result =
                    (result >> 8) ^ CRCtable64[static_cast<uint8_t>(result) ^
                                               static_cast<uint8_t>(data[i])];
            }
            return result;
        }

        static constexpr uint64_t CreateCRC64(std::string_view str) noexcept {
            return CreateCRC64(str.data(), str.size());
        }

        static constexpr uint32_t CRC32_Init() noexcept { return 0xFFFFFFFFu; }

        static constexpr uint32_t CRC32_Update(uint32_t crc, const void *data,
                                               size_t size) noexcept {
            const auto *ptr = static_cast<const uint8_t *>(data);
            for (size_t i = 0; i < size; ++i) {
                crc = (crc >> 8) ^ CRCtable32[(crc ^ ptr[i]) & 0xFF];
            }
            return crc;
        }

        static constexpr uint32_t CRC32_Finalize(uint32_t crc) noexcept {
            return crc ^ 0xFFFFFFFFu;
        }

        static constexpr uint32_t CreateCRC(std::string_view str) noexcept {
            uint32_t crc = CRC32_Init();
            crc = CRC32_Update(crc, str.data(), str.size());
            return CRC32_Finalize(crc);
        }

        static constexpr uint32_t
        CreateCRCNoCase(std::string_view str) noexcept {
            uint32_t crc = CRC32_Init();
            for (char c : str) {
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c + 32);
                }
                crc = (crc >> 8) ^
                      CRCtable32[(crc ^ static_cast<uint8_t>(c)) & 0xFF];
            }
            return CRC32_Finalize(crc);
        }

        // FNV-1a
        static constexpr uint64_t InternalFnv64(const char *data,
                                                size_t length) noexcept {
            uint64_t hash = 0xcbf29ce484222325ULL;
            for (size_t i = 0; i < length; ++i) {
                hash *= 0x100000001b3ULL;
                hash ^= static_cast<uint8_t>(data[i]);
            }
            return hash;
        }

        static constexpr uint64_t InternalFnv64(std::string_view str) noexcept {
            return InternalFnv64(str.data(), str.size());
        }

        static constexpr uint64_t CreateFnv64(const char *data,
                                              size_t length) noexcept {
            uint64_t hash64 = InternalFnv64(data, length);
            hash64 = (hash64 & 0x1FFFFFFFFFFFFFFFULL) | 0xA000000000000000ULL;
            return hash64;
        }

        static constexpr uint64_t CreateFnv64(std::string_view str) noexcept {
            return CreateFnv64(str.data(), str.size());
        }

        static constexpr uint32_t
        GenerateHash32(std::string_view str) noexcept {
            uint64_t hash64 = CreateFnv64(str);
            uint32_t hash32 = static_cast<uint32_t>(hash64 & 0xFFFFFFFFULL);
            if ((hash32 & 0xFFFF0000U) == 0xFFFF0000U) {
                hash32 &= ~(1U << 16);
            }
            return hash32;
        }

        // used for FATv2
        static constexpr uint64_t
        ComputeCRC64Path(std::string_view name) noexcept {
            size_t start = 0;
            while (start < name.size() &&
                   (name[start] == '/' || name[start] == '\\')) {
                ++start;
            }
            if (start >= name.size()) {
                return 0ULL;
            }
            uint64_t hash = 0ULL;
            for (size_t i = start; i < name.size(); ++i) {
                char c = name[i];
                if (c == '/') {
                    c = '\\';
                }
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c + 32);
                }
                hash = (hash >> 8) ^ CRCtable64[static_cast<uint8_t>(hash) ^
                                                static_cast<uint8_t>(c)];
            }
            return hash;
        }

        // used for FATv3 (32bit)
        static constexpr uint32_t
        ComputeFNV32Path(std::string_view name) noexcept {
            size_t start = 0;
            while (start < name.size() &&
                   (name[start] == '/' || name[start] == '\\')) {
                ++start;
            }
            if (start >= name.size()) {
                return 0xFFFFFFFFu;
            }
            uint64_t hash64 = 0xcbf29ce484222325ULL;
            for (size_t i = start; i < name.size(); ++i) {
                char c = name[i];
                if (c == '/') {
                    c = '\\';
                }
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c + 32);
                }
                hash64 *= 0x100000001b3ULL;
                hash64 ^= static_cast<uint8_t>(c);
            }
            hash64 = (hash64 & 0x1FFFFFFFFFFFFFFFULL) | 0xA000000000000000ULL;
            uint32_t hash32 = static_cast<uint32_t>(hash64 & 0xFFFFFFFFULL);
            if ((hash32 & 0xFFFF0000U) == 0xFFFF0000U) {
                hash32 &= ~(1U << 16);
            }
            return hash32;
        }

        // used for FATv5 (64bit)
        static constexpr uint64_t
        ComputeFNV64Path(std::string_view name) noexcept {
            size_t start = 0;
            while (start < name.size() &&
                   (name[start] == '/' || name[start] == '\\')) {
                ++start;
            }
            if (start >= name.size()) {
                return 0xFFFFFFFFFFFFFFFFULL;
            }
            uint64_t hash = 0xcbf29ce484222325ULL;
            for (size_t i = start; i < name.size(); ++i) {
                char c = name[i];
                if (c == '/') {
                    c = '\\';
                }
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c + 32);
                }
                hash *= 0x100000001b3ULL;
                hash ^= static_cast<uint8_t>(c);
            }
            hash = (hash & 0x1FFFFFFFFFFFFFFFULL) | 0xA000000000000000ULL;
            return hash;
        }
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_HASHINGUTILS_H
