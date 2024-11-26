// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


const unsigned char g_main_ps[] = {0x44, 0x58, 0x42, 0x43, 0x86, 0xc2, 0x8d, 0x1e, 0x38, 0x59, 0x5e, 0xc8, 0x57, 0xd8, 0xd0, 0xc3, 0xb6,
  0x0e, 0x2f, 0xd5, 0x01, 0x00, 0x00, 0x00, 0xcb, 0x06, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00,
  0x00, 0xa9, 0x00, 0x00, 0x00, 0xe3, 0x00, 0x00, 0x00, 0x83, 0x01, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30, 0x08, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x31, 0x5d, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53,
  0x56, 0x5f, 0x50, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x00, 0x4f, 0x53,
  0x47, 0x31, 0x32, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x53, 0x56, 0x5f, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x50, 0x53, 0x56, 0x30, 0x98, 0x00, 0x00, 0x00, 0x24, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00,
  0x00, 0x00, 0x00, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x44, 0x03, 0x03, 0x04, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x44, 0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x44, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x44, 0x58, 0x49, 0x4c,
  0x40, 0x05, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x50, 0x01, 0x00, 0x00, 0x44, 0x58, 0x49, 0x4c, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00,
  0x00, 0x00, 0x28, 0x05, 0x00, 0x00, 0x42, 0x43, 0xc0, 0xde, 0x21, 0x0c, 0x00, 0x00, 0x47, 0x01, 0x00, 0x00, 0x0b, 0x82, 0x20, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x07, 0x81, 0x23, 0x91, 0x41, 0xc8, 0x04, 0x49, 0x06, 0x10, 0x32, 0x39, 0x92, 0x01,
  0x84, 0x0c, 0x25, 0x05, 0x08, 0x19, 0x1e, 0x04, 0x8b, 0x62, 0x80, 0x10, 0x45, 0x02, 0x42, 0x92, 0x0b, 0x42, 0x84, 0x10, 0x32, 0x14,
  0x38, 0x08, 0x18, 0x4b, 0x0a, 0x32, 0x42, 0x88, 0x48, 0x90, 0x14, 0x20, 0x43, 0x46, 0x88, 0xa5, 0x00, 0x19, 0x32, 0x42, 0xe4, 0x48,
  0x0e, 0x90, 0x11, 0x22, 0xc4, 0x50, 0x41, 0x51, 0x81, 0x8c, 0xe1, 0x83, 0xe5, 0x8a, 0x04, 0x21, 0x46, 0x06, 0x51, 0x18, 0x00, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x1b, 0x8c, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x07, 0x40, 0x02, 0xa8, 0x0d, 0x84, 0xf0, 0xff, 0xff, 0xff, 0xff,
  0x03, 0x20, 0x01, 0x00, 0x00, 0x00, 0x49, 0x18, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x13, 0x82, 0x60, 0x42, 0x20, 0x00, 0x00, 0x00,
  0x89, 0x20, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x32, 0x22, 0x08, 0x09, 0x20, 0x64, 0x85, 0x04, 0x13, 0x22, 0xa4, 0x84, 0x04, 0x13,
  0x22, 0xe3, 0x84, 0xa1, 0x90, 0x14, 0x12, 0x4c, 0x88, 0x8c, 0x0b, 0x84, 0x84, 0x4c, 0x10, 0x38, 0x23, 0x00, 0x25, 0x00, 0x8a, 0x19,
  0x80, 0x39, 0x02, 0x30, 0x98, 0x23, 0x40, 0x8a, 0x31, 0x44, 0x54, 0x44, 0x56, 0x0c, 0x20, 0xa2, 0x1a, 0xc2, 0x81, 0x80, 0x61, 0x04,
  0x62, 0x38, 0x45, 0x9a, 0x22, 0x4a, 0x98, 0xfc, 0xca, 0x7e, 0x90, 0xb0, 0x9b, 0x0a, 0x04, 0x00, 0x00, 0x00, 0x13, 0x14, 0x72, 0xc0,
  0x87, 0x74, 0x60, 0x87, 0x36, 0x68, 0x87, 0x79, 0x68, 0x03, 0x72, 0xc0, 0x87, 0x0d, 0xaf, 0x50, 0x0e, 0x6d, 0xd0, 0x0e, 0x7a, 0x50,
  0x0e, 0x6d, 0x00, 0x0f, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0xa0, 0x07, 0x73, 0x20, 0x07,
  0x6d, 0x90, 0x0e, 0x78, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe9,
  0x30, 0x07, 0x72, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x76, 0x40, 0x07, 0x7a, 0x60, 0x07, 0x74, 0xd0, 0x06, 0xe6, 0x10,
  0x07, 0x76, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x60, 0x0e, 0x73, 0x20, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe6, 0x60, 0x07,
  0x74, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x6d, 0xe0, 0x0e, 0x78, 0xa0, 0x07, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x76,
  0x40, 0x07, 0x43, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x3c, 0x06, 0x10, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x79, 0x10, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x02, 0x01,
  0x0e, 0x00, 0x00, 0x00, 0x32, 0x1e, 0x98, 0x14, 0x19, 0x11, 0x4c, 0x90, 0x8c, 0x09, 0x26, 0x47, 0xc6, 0x04, 0x43, 0xa2, 0x12, 0x18,
  0x01, 0x28, 0x84, 0x62, 0x28, 0xa0, 0x32, 0x28, 0x87, 0x92, 0x28, 0x10, 0xaa, 0x92, 0x28, 0x83, 0x42, 0x18, 0x01, 0x28, 0x82, 0x02,
  0x21, 0x9e, 0x01, 0xa0, 0x1e, 0x4b, 0x41, 0x08, 0x00, 0x00, 0x80, 0x40, 0x20, 0x10, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x61, 0x00,
  0x00, 0x00, 0x1a, 0x03, 0x4c, 0x90, 0x46, 0x02, 0x13, 0xc4, 0x83, 0x0c, 0x6f, 0x0c, 0x24, 0xc6, 0x45, 0x66, 0x43, 0x10, 0x4c, 0x10,
  0x88, 0x61, 0x82, 0x40, 0x10, 0x1b, 0x84, 0x81, 0x98, 0x20, 0x10, 0xc5, 0x06, 0x61, 0x30, 0x28, 0xc0, 0xcd, 0x4d, 0x10, 0x08, 0x63,
  0xc3, 0x80, 0x24, 0xc4, 0x04, 0x81, 0x91, 0x26, 0x08, 0xc4, 0xc1, 0x01, 0xee, 0x6d, 0x6e, 0x82, 0x40, 0x20, 0x5c, 0xa6, 0xac, 0xbe,
  0xa0, 0x9e, 0xa6, 0x92, 0xa8, 0x92, 0x9e, 0x9c, 0x26, 0x08, 0x44, 0x32, 0x41, 0x20, 0x94, 0x0d, 0x48, 0xd2, 0x38, 0x84, 0xf1, 0x40,
  0x11, 0x87, 0xb1, 0x37, 0xb6, 0x09, 0x02, 0xb1, 0x30, 0xa1, 0x2a, 0xc2, 0x1a, 0x7a, 0x7a, 0x92, 0x22, 0x82, 0xd9, 0x80, 0x24, 0x93,
  0x43, 0x19, 0x15, 0x14, 0x6d, 0x18, 0x18, 0xc9, 0xda, 0x30, 0x10, 0xcb, 0x35, 0x41, 0x10, 0x80, 0x0d, 0xc0, 0x86, 0x81, 0xd0, 0xb4,
  0x0d, 0xc1, 0xb6, 0x61, 0x18, 0x32, 0x6e, 0x82, 0xd0, 0x4c, 0x1b, 0x02, 0x8f, 0x44, 0x5b, 0x58, 0x9a, 0x1b, 0x97, 0x29, 0xab, 0x2f,
  0xa8, 0xb7, 0xb9, 0x34, 0xba, 0xb4, 0x37, 0xb7, 0x09, 0x42, 0xc1, 0x4c, 0x10, 0x8a, 0x66, 0x43, 0x40, 0x4c, 0x10, 0x0a, 0x67, 0x82,
  0x50, 0x3c, 0x1b, 0x16, 0x22, 0x0c, 0xc4, 0x60, 0x0c, 0xc8, 0xa0, 0x0c, 0x86, 0x32, 0x20, 0xcc, 0x00, 0x20, 0x42, 0x55, 0x84, 0x35,
  0xf4, 0xf4, 0x24, 0x45, 0x34, 0x41, 0x28, 0xa0, 0x0d, 0xcb, 0x80, 0x06, 0x62, 0x60, 0x06, 0x64, 0x90, 0x06, 0x43, 0x19, 0x0c, 0x66,
  0x00, 0x6c, 0x10, 0xce, 0x40, 0x0d, 0x98, 0x4c, 0x59, 0x7d, 0x51, 0x85, 0xc9, 0x9d, 0x95, 0xd1, 0x4d, 0x10, 0x8a, 0x68, 0xc3, 0x42,
  0xb0, 0x81, 0x18, 0xb4, 0x01, 0x19, 0x98, 0xc1, 0x50, 0x06, 0x84, 0x19, 0x00, 0x1b, 0x02, 0x37, 0xd8, 0x30, 0xac, 0xc1, 0x1b, 0x00,
  0x1b, 0x8a, 0x0c, 0x0c, 0xe0, 0x00, 0x00, 0xaa, 0xb0, 0xb1, 0xd9, 0xb5, 0xb9, 0xa4, 0x91, 0x95, 0xb9, 0xd1, 0x4d, 0x09, 0x82, 0x2a,
  0x64, 0x78, 0x2e, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x02, 0xa2, 0x09, 0x19, 0x9e, 0x8b, 0x5d, 0x18, 0x9b, 0x5d, 0x99,
  0xdc, 0x94, 0xc0, 0xa8, 0x43, 0x86, 0xe7, 0x32, 0x87, 0x16, 0x46, 0x56, 0x26, 0xd7, 0xf4, 0x46, 0x56, 0xc6, 0x36, 0x25, 0x48, 0x2a,
  0x91, 0xe1, 0xb9, 0xd0, 0xe5, 0xc1, 0x95, 0x05, 0xb9, 0xb9, 0xbd, 0xd1, 0x85, 0xd1, 0xa5, 0xbd, 0xb9, 0xcd, 0x4d, 0x11, 0x2e, 0xae,
  0x0e, 0x19, 0x9e, 0x8b, 0x5d, 0x5a, 0xd9, 0x5d, 0x12, 0xd9, 0x14, 0x5d, 0x18, 0x5d, 0xd9, 0x94, 0xc0, 0xab, 0x43, 0x86, 0xe7, 0x52,
  0xe6, 0x46, 0x27, 0x97, 0x07, 0xf5, 0x96, 0xe6, 0x46, 0x37, 0x37, 0x25, 0x80, 0x03, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x42, 0x00,
  0x00, 0x00, 0x33, 0x08, 0x80, 0x1c, 0xc4, 0xe1, 0x1c, 0x66, 0x14, 0x01, 0x3d, 0x88, 0x43, 0x38, 0x84, 0xc3, 0x8c, 0x42, 0x80, 0x07,
  0x79, 0x78, 0x07, 0x73, 0x98, 0x71, 0x0c, 0xe6, 0x00, 0x0f, 0xed, 0x10, 0x0e, 0xf4, 0x80, 0x0e, 0x33, 0x0c, 0x42, 0x1e, 0xc2, 0xc1,
  0x1d, 0xce, 0xa1, 0x1c, 0x66, 0x30, 0x05, 0x3d, 0x88, 0x43, 0x38, 0x84, 0x83, 0x1b, 0xcc, 0x03, 0x3d, 0xc8, 0x43, 0x3d, 0x8c, 0x03,
  0x3d, 0xcc, 0x78, 0x8c, 0x74, 0x70, 0x07, 0x7b, 0x08, 0x07, 0x79, 0x48, 0x87, 0x70, 0x70, 0x07, 0x7a, 0x70, 0x03, 0x76, 0x78, 0x87,
  0x70, 0x20, 0x87, 0x19, 0xcc, 0x11, 0x0e, 0xec, 0x90, 0x0e, 0xe1, 0x30, 0x0f, 0x6e, 0x30, 0x0f, 0xe3, 0xf0, 0x0e, 0xf0, 0x50, 0x0e,
  0x33, 0x10, 0xc4, 0x1d, 0xde, 0x21, 0x1c, 0xd8, 0x21, 0x1d, 0xc2, 0x61, 0x1e, 0x66, 0x30, 0x89, 0x3b, 0xbc, 0x83, 0x3b, 0xd0, 0x43,
  0x39, 0xb4, 0x03, 0x3c, 0xbc, 0x83, 0x3c, 0x84, 0x03, 0x3b, 0xcc, 0xf0, 0x14, 0x76, 0x60, 0x07, 0x7b, 0x68, 0x07, 0x37, 0x68, 0x87,
  0x72, 0x68, 0x07, 0x37, 0x80, 0x87, 0x70, 0x90, 0x87, 0x70, 0x60, 0x07, 0x76, 0x28, 0x07, 0x76, 0xf8, 0x05, 0x76, 0x78, 0x87, 0x77,
  0x80, 0x87, 0x5f, 0x08, 0x87, 0x71, 0x18, 0x87, 0x72, 0x98, 0x87, 0x79, 0x98, 0x81, 0x2c, 0xee, 0xf0, 0x0e, 0xee, 0xe0, 0x0e, 0xf5,
  0xc0, 0x0e, 0xec, 0x30, 0x03, 0x62, 0xc8, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xcc, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xdc, 0x61, 0x1c, 0xca,
  0x21, 0x1c, 0xc4, 0x81, 0x1d, 0xca, 0x61, 0x06, 0xd6, 0x90, 0x43, 0x39, 0xc8, 0x43, 0x39, 0x98, 0x43, 0x39, 0xc8, 0x43, 0x39, 0xb8,
  0xc3, 0x38, 0x94, 0x43, 0x38, 0x88, 0x03, 0x3b, 0x94, 0xc3, 0x2f, 0xbc, 0x83, 0x3c, 0xfc, 0x82, 0x3b, 0xd4, 0x03, 0x3b, 0xb0, 0x03,
  0x00, 0x00, 0x71, 0x20, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x16, 0x30, 0x0d, 0x97, 0xef, 0x3c, 0xfe, 0xe2, 0x00, 0x83, 0xd8, 0x3c,
  0xd4, 0xe4, 0x17, 0xb7, 0x6d, 0x00, 0x04, 0x03, 0x20, 0x8d, 0x09, 0x54, 0xc3, 0xe5, 0x3b, 0x8f, 0x2f, 0x4d, 0x4e, 0x44, 0xa0, 0xd4,
  0xf4, 0x50, 0x93, 0x5f, 0xdc, 0x36, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x13, 0x04, 0x41, 0x2c,
  0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x44, 0x33, 0x00, 0xa5, 0x40, 0x55, 0x02, 0x00, 0x23, 0x06, 0x09, 0x00, 0x82, 0x60,
  0x60, 0x58, 0x12, 0x35, 0x21, 0xc3, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x18, 0xd7, 0x54, 0x51, 0x02, 0x31, 0x62, 0x90, 0x00, 0x20,
  0x08, 0x06, 0x06, 0x46, 0x59, 0x55, 0x52, 0x8c, 0x18, 0x24, 0x00, 0x08, 0x82, 0x81, 0x91, 0x55, 0x97, 0xd5, 0x18, 0x23, 0x06, 0x09,
  0x00, 0x82, 0x60, 0x80, 0x64, 0xc6, 0x75, 0x31, 0xc4, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x20, 0x99, 0x71, 0x5d, 0xc5, 0x30, 0x62,
  0x90, 0x00, 0x20, 0x08, 0x06, 0x48, 0x66, 0x5c, 0xd7, 0x22, 0x8c, 0x18, 0x24, 0x00, 0x08, 0x82, 0x01, 0x92, 0x19, 0xd7, 0xe5, 0x04,
  0x08, 0x00, 0x00, 0x00, 0x00, 0x00};


static const uint8_t shaderps[2956] =
{
    0x44, 0x58, 0x42, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // DXBC............
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x8c, 0x0b, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,  // ................
    0x3c, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0xec, 0x00, 0x00, 0x00,  // <...L...........
    0x94, 0x01, 0x00, 0x00, 0x70, 0x06, 0x00, 0x00, 0x8c, 0x06, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30,  // ....p.......SFI0
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x31,  // ............ISG1
    0x5c, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // \...............
    0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,  // H...............
    0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ................
    0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,  // T...............
    0x01, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x56, 0x5f, 0x50,  // ............SV_P
    0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x43, 0x4f, 0x4c, 0x4f, 0x52, 0x00, 0x00, 0x00,  // osition.COLOR...
    0x4f, 0x53, 0x47, 0x31, 0x34, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,  // OSG14...........
    0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,  // ....(.......@...
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ................
    0x53, 0x56, 0x5f, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x00, 0x00, 0x50, 0x53, 0x56, 0x30,  // SV_Target...PSV0
    0xa0, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ....0...........
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,  // ................
    0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ................
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,  // ................
    0x00, 0x43, 0x4f, 0x4c, 0x4f, 0x52, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // .COLOR..........
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x44, 0x03,  // ..............D.
    0x03, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x44, 0x00,  // ..............D.
    0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x44, 0x10,  // ..............D.
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ................
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,  // ................
    0x08, 0x00, 0x00, 0x00, 0x53, 0x54, 0x41, 0x54, 0xd4, 0x04, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,  // ....STAT....`...
    0x35, 0x01, 0x00, 0x00, 0x44, 0x58, 0x49, 0x4c, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,  // 5...DXIL........
    0xbc, 0x04, 0x00, 0x00, 0x42, 0x43, 0xc0, 0xde, 0x21, 0x0c, 0x00, 0x00, 0x2c, 0x01, 0x00, 0x00,  // ....BC..!...,...
    0x0b, 0x82, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x07, 0x81, 0x23, 0x91,  // .. ...........#.
    0x41, 0xc8, 0x04, 0x49, 0x06, 0x10, 0x32, 0x39, 0x92, 0x01, 0x84, 0x0c, 0x25, 0x05, 0x08, 0x19,  // A..I..29....%...
    0x1e, 0x04, 0x8b, 0x62, 0x80, 0x10, 0x45, 0x02, 0x42, 0x92, 0x0b, 0x42, 0x84, 0x10, 0x32, 0x14,  // ...b..E.B..B..2.
    0x38, 0x08, 0x18, 0x4b, 0x0a, 0x32, 0x42, 0x88, 0x48, 0x90, 0x14, 0x20, 0x43, 0x46, 0x88, 0xa5,  // 8..K.2B.H.. CF..
    0x00, 0x19, 0x32, 0x42, 0xe4, 0x48, 0x0e, 0x90, 0x11, 0x22, 0xc4, 0x50, 0x41, 0x51, 0x81, 0x8c,  // ..2B.H...".PAQ..
    0xe1, 0x83, 0xe5, 0x8a, 0x04, 0x21, 0x46, 0x06, 0x51, 0x18, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,  // .....!F.Q.......
    0x1b, 0x8c, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x07, 0x40, 0x02, 0xa8, 0x0d, 0x84, 0xf0, 0xff, 0xff,  // ........@.......
    0xff, 0xff, 0x03, 0x20, 0x01, 0x00, 0x00, 0x00, 0x49, 0x18, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,  // ... ....I.......
    0x13, 0x82, 0x60, 0x42, 0x20, 0x00, 0x00, 0x00, 0x89, 0x20, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,  // ..`B .... ......
    0x32, 0x22, 0x08, 0x09, 0x20, 0x64, 0x85, 0x04, 0x13, 0x22, 0xa4, 0x84, 0x04, 0x13, 0x22, 0xe3,  // 2".. d..."....".
    0x84, 0xa1, 0x90, 0x14, 0x12, 0x4c, 0x88, 0x8c, 0x0b, 0x84, 0x84, 0x4c, 0x10, 0x30, 0x23, 0x00,  // .....L.....L.0#.
    0x25, 0x00, 0x8a, 0x19, 0x80, 0x39, 0x02, 0x30, 0x98, 0x23, 0x40, 0x8a, 0x31, 0x44, 0x54, 0x44,  // %....9.0.#@.1DTD
    0x56, 0x0c, 0x20, 0xa2, 0x1a, 0xc2, 0x81, 0x80, 0x54, 0x20, 0x00, 0x00, 0x13, 0x14, 0x72, 0xc0,  // V. .....T ....r.
    0x87, 0x74, 0x60, 0x87, 0x36, 0x68, 0x87, 0x79, 0x68, 0x03, 0x72, 0xc0, 0x87, 0x0d, 0xaf, 0x50,  // .t`.6h.yh.r....P
    0x0e, 0x6d, 0xd0, 0x0e, 0x7a, 0x50, 0x0e, 0x6d, 0x00, 0x0f, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07,  // .m..zP.m..z0.r..
    0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x78,  // s .m..q..s .m..x
    0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0,  // ..s .m..q`.z0.r.
    0x06, 0xe9, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x76, 0x40, 0x07,  // ..0.r..s .m..v@.
    0x7a, 0x60, 0x07, 0x74, 0xd0, 0x06, 0xe6, 0x10, 0x07, 0x76, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d,  // z`.t.....v..s .m
    0x60, 0x0e, 0x73, 0x20, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe6, 0x60, 0x07, 0x74, 0xa0,  // `.s .z0.r...`.t.
    0x07, 0x76, 0x40, 0x07, 0x6d, 0xe0, 0x0e, 0x78, 0xa0, 0x07, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07,  // .v@.m..x..q`.z0.
    0x72, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x43, 0x9e, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // r..v@.C.........
    0x00, 0x00, 0x00, 0x86, 0x3c, 0x06, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ....<...........
    0x0c, 0x79, 0x10, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x02, 0x01,  // .y. ............
    0x0d, 0x00, 0x00, 0x00, 0x32, 0x1e, 0x98, 0x14, 0x19, 0x11, 0x4c, 0x90, 0x8c, 0x09, 0x26, 0x47,  // ....2.....L...&G
    0xc6, 0x04, 0x43, 0xa2, 0x12, 0x18, 0x01, 0x28, 0x86, 0x32, 0x28, 0x8f, 0x92, 0x28, 0x04, 0xaa,  // ..C....(.2(..(..
    0x92, 0x28, 0x83, 0x42, 0x18, 0x01, 0x28, 0x82, 0x02, 0xa1, 0x1d, 0x4b, 0x41, 0x08, 0x00, 0x00,  // .(.B..(....KA...
    0x80, 0x40, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,  // .@ .....y...`...
    0x1a, 0x03, 0x4c, 0x90, 0x46, 0x02, 0x13, 0xc4, 0x31, 0x20, 0xc3, 0x1b, 0x43, 0x81, 0x93, 0x4b,  // ..L.F...1 ..C..K
    0xb3, 0x0b, 0xa3, 0x2b, 0x4b, 0x01, 0x89, 0x71, 0xb9, 0x71, 0x81, 0x71, 0x99, 0xc9, 0x99, 0xc9,  // ...+K..q.q.q....
    0x01, 0x41, 0xa9, 0x29, 0x83, 0xc1, 0x81, 0x09, 0xbb, 0xb9, 0x91, 0x49, 0xd9, 0x10, 0x04, 0x13,  // .A.).......I....
    0x04, 0x62, 0x98, 0x20, 0x10, 0xc4, 0x06, 0x61, 0x20, 0x36, 0x08, 0x04, 0x41, 0x01, 0x6e, 0x6e,  // .b. ...a 6..A.nn
    0x82, 0x40, 0x14, 0x1b, 0x86, 0x03, 0x21, 0x26, 0x08, 0x02, 0xb0, 0x01, 0xd8, 0x30, 0x10, 0xcb,  // .@....!&.....0..
    0xb2, 0x21, 0x60, 0x36, 0x0c, 0x83, 0xd2, 0x4c, 0x10, 0x16, 0x68, 0x43, 0xf0, 0xd0, 0x80, 0x9a,  // .!`6...L..hC....
    0x6a, 0x0a, 0x4b, 0x73, 0xe3, 0x32, 0x65, 0xf5, 0x05, 0xf5, 0x36, 0x97, 0x46, 0x97, 0xf6, 0xe6,  // j.Ks.2e...6.F...
    0x36, 0x41, 0x28, 0x94, 0x09, 0x42, 0xb1, 0x6c, 0x08, 0x88, 0x09, 0x42, 0xc1, 0x4c, 0x10, 0x8a,  // 6A(..B.l...B.L..
    0x66, 0xc3, 0x42, 0x48, 0x13, 0x55, 0x59, 0x83, 0x45, 0x5c, 0x00, 0x8b, 0xa1, 0x27, 0xa6, 0x27,  // f.BH.UY.E\...'.'
    0xa9, 0x09, 0x42, 0xe1, 0x4c, 0x10, 0x08, 0x63, 0x82, 0x40, 0x1c, 0x1b, 0x84, 0x8d, 0xdb, 0xb0,  // ..B.L..c.@......
    0x0c, 0xd9, 0x74, 0x55, 0xda, 0x60, 0x0d, 0x57, 0xb7, 0x41, 0xc0, 0x3c, 0x26, 0x53, 0x56, 0x5f,  // ..tU.`.W.A.<&SV_
    0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x13, 0x84, 0xe2, 0xd9, 0xb0, 0x10, 0x60, 0x30, 0x85, 0x41,  // Target......`0.A
    0x75, 0x0d, 0x16, 0x71, 0x75, 0x1b, 0x02, 0x31, 0xd8, 0x30, 0x7c, 0x63, 0x00, 0x6c, 0x28, 0x94,  // u..qu..1.0|c.l(.
    0x88, 0x0c, 0x00, 0x80, 0x45, 0x9a, 0xdb, 0x1c, 0xdd, 0xdc, 0x04, 0x81, 0x40, 0x68, 0xcc, 0xa5,  // ....E.......@h..
    0x9d, 0x7d, 0xb1, 0x91, 0x4d, 0x10, 0x88, 0x84, 0xc6, 0x5c, 0xda, 0xd9, 0xd7, 0x1c, 0xdd, 0x06,  // .}..M....\......
    0xc3, 0x0c, 0xce, 0x00, 0x0d, 0xd2, 0x40, 0x0d, 0xd2, 0xa0, 0x0a, 0x1b, 0x9b, 0x5d, 0x9b, 0x4b,  // ......@......].K
    0x1a, 0x59, 0x99, 0x1b, 0xdd, 0x94, 0x20, 0xa8, 0x42, 0x86, 0xe7, 0x62, 0x57, 0x26, 0x37, 0x97,  // .Y.... .B..bW&7.
    0xf6, 0xe6, 0x36, 0x25, 0x20, 0x9a, 0x90, 0xe1, 0xb9, 0xd8, 0x85, 0xb1, 0xd9, 0x95, 0xc9, 0x4d,  // ..6% ..........M
    0x09, 0x8a, 0x3a, 0x64, 0x78, 0x2e, 0x73, 0x68, 0x61, 0x64, 0x65, 0x72, 0x4d, 0x6f, 0x64, 0x65,  // ..:dx.shaderMode
    0x6c, 0x53, 0x02, 0xa4, 0x12, 0x19, 0x9e, 0x0b, 0x5d, 0x1e, 0x5c, 0x59, 0x90, 0x9b, 0xdb, 0x1b,  // lS......].\Y....
    0x5d, 0x18, 0x5d, 0xda, 0x9b, 0xdb, 0xdc, 0x94, 0xa0, 0xa9, 0x43, 0x86, 0xe7, 0x62, 0x97, 0x56,  // ].].......C..b.V
    0x76, 0x97, 0x44, 0x36, 0x45, 0x17, 0x46, 0x57, 0x36, 0x25, 0x78, 0xea, 0x90, 0xe1, 0xb9, 0x94,  // v.D6E.FW6%x.....
    0xb9, 0xd1, 0xc9, 0xe5, 0x41, 0xbd, 0xa5, 0xb9, 0xd1, 0xcd, 0x4d, 0x09, 0xc8, 0xa0, 0x0b, 0x19,  // ....A.....M.....
    0x9e, 0xcb, 0xd8, 0x5b, 0x9d, 0x1b, 0x5d, 0x99, 0xdc, 0xdc, 0x94, 0x40, 0x0d, 0x00, 0x00, 0x00,  // ...[..]....@....
    0x79, 0x18, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x33, 0x08, 0x80, 0x1c, 0xc4, 0xe1, 0x1c, 0x66,  // y...L...3......f
    0x14, 0x01, 0x3d, 0x88, 0x43, 0x38, 0x84, 0xc3, 0x8c, 0x42, 0x80, 0x07, 0x79, 0x78, 0x07, 0x73,  // ..=.C8...B..yx.s
    0x98, 0x71, 0x0c, 0xe6, 0x00, 0x0f, 0xed, 0x10, 0x0e, 0xf4, 0x80, 0x0e, 0x33, 0x0c, 0x42, 0x1e,  // .q..........3.B.
    0xc2, 0xc1, 0x1d, 0xce, 0xa1, 0x1c, 0x66, 0x30, 0x05, 0x3d, 0x88, 0x43, 0x38, 0x84, 0x83, 0x1b,  // ......f0.=.C8...
    0xcc, 0x03, 0x3d, 0xc8, 0x43, 0x3d, 0x8c, 0x03, 0x3d, 0xcc, 0x78, 0x8c, 0x74, 0x70, 0x07, 0x7b,  // ..=.C=..=.x.tp.{
    0x08, 0x07, 0x79, 0x48, 0x87, 0x70, 0x70, 0x07, 0x7a, 0x70, 0x03, 0x76, 0x78, 0x87, 0x70, 0x20,  // ..yH.pp.zp.vx.p 
    0x87, 0x19, 0xcc, 0x11, 0x0e, 0xec, 0x90, 0x0e, 0xe1, 0x30, 0x0f, 0x6e, 0x30, 0x0f, 0xe3, 0xf0,  // .........0.n0...
    0x0e, 0xf0, 0x50, 0x0e, 0x33, 0x10, 0xc4, 0x1d, 0xde, 0x21, 0x1c, 0xd8, 0x21, 0x1d, 0xc2, 0x61,  // ..P.3....!..!..a
    0x1e, 0x66, 0x30, 0x89, 0x3b, 0xbc, 0x83, 0x3b, 0xd0, 0x43, 0x39, 0xb4, 0x03, 0x3c, 0xbc, 0x83,  // .f0.;..;.C9..<..
    0x3c, 0x84, 0x03, 0x3b, 0xcc, 0xf0, 0x14, 0x76, 0x60, 0x07, 0x7b, 0x68, 0x07, 0x37, 0x68, 0x87,  // <..;...v`.{h.7h.
    0x72, 0x68, 0x07, 0x37, 0x80, 0x87, 0x70, 0x90, 0x87, 0x70, 0x60, 0x07, 0x76, 0x28, 0x07, 0x76,  // rh.7..p..p`.v(.v
    0xf8, 0x05, 0x76, 0x78, 0x87, 0x77, 0x80, 0x87, 0x5f, 0x08, 0x87, 0x71, 0x18, 0x87, 0x72, 0x98,  // ..vx.w.._..q..r.
    0x87, 0x79, 0x98, 0x81, 0x2c, 0xee, 0xf0, 0x0e, 0xee, 0xe0, 0x0e, 0xf5, 0xc0, 0x0e, 0xec, 0x30,  // .y..,..........0
    0x03, 0x62, 0xc8, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xcc, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xdc, 0x61,  // .b.............a
    0x1c, 0xca, 0x21, 0x1c, 0xc4, 0x81, 0x1d, 0xca, 0x61, 0x06, 0xd6, 0x90, 0x43, 0x39, 0xc8, 0x43,  // ..!.....a...C9.C
    0x39, 0x98, 0x43, 0x39, 0xc8, 0x43, 0x39, 0xb8, 0xc3, 0x38, 0x94, 0x43, 0x38, 0x88, 0x03, 0x3b,  // 9.C9.C9..8.C8..;
    0x94, 0xc3, 0x2f, 0xbc, 0x83, 0x3c, 0xfc, 0x82, 0x3b, 0xd4, 0x03, 0x3b, 0xb0, 0xc3, 0x0c, 0xc4,  // ../..<..;..;....
    0x21, 0x07, 0x7c, 0x70, 0x03, 0x7a, 0x28, 0x87, 0x76, 0x80, 0x87, 0x19, 0xd1, 0x43, 0x0e, 0xf8,  // !.|p.z(.v....C..
    0xe0, 0x06, 0xe4, 0x20, 0x0e, 0xe7, 0xe0, 0x06, 0xf6, 0x10, 0x0e, 0xf2, 0xc0, 0x0e, 0xe1, 0x90,  // ... ............
    0x0f, 0xef, 0x50, 0x0f, 0xf4, 0x00, 0x00, 0x00, 0x71, 0x20, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,  // ..P.....q ......
    0x06, 0x60, 0xa4, 0xac, 0x09, 0x20, 0x8d, 0x05, 0x4c, 0xc3, 0xe5, 0x3b, 0x8f, 0xbf, 0x38, 0xc0,  // .`... ..L..;..8.
    0x20, 0x36, 0x0f, 0x35, 0xf9, 0xc5, 0x6d, 0x9b, 0x40, 0x35, 0x5c, 0xbe, 0xf3, 0xf8, 0xd2, 0xe4,  //  6.5..m.@5\.....
    0x44, 0x04, 0x4a, 0x4d, 0x0f, 0x35, 0xf9, 0xc5, 0x6d, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // D.JM.5..m.......
    0x48, 0x41, 0x53, 0x48, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x19, 0xa4, 0xbe,  // HASH........6...
    0xec, 0x45, 0x61, 0xe7, 0xdc, 0xbe, 0x00, 0xdc, 0x13, 0x7f, 0x6f, 0xc0, 0x44, 0x58, 0x49, 0x4c,  // .Ea.......o.DXIL
    0xf8, 0x04, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x3e, 0x01, 0x00, 0x00, 0x44, 0x58, 0x49, 0x4c,  // ....`...>...DXIL
    0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xe0, 0x04, 0x00, 0x00, 0x42, 0x43, 0xc0, 0xde,  // ............BC..
    0x21, 0x0c, 0x00, 0x00, 0x35, 0x01, 0x00, 0x00, 0x0b, 0x82, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00,  // !...5..... .....
    0x13, 0x00, 0x00, 0x00, 0x07, 0x81, 0x23, 0x91, 0x41, 0xc8, 0x04, 0x49, 0x06, 0x10, 0x32, 0x39,  // ......#.A..I..29
    0x92, 0x01, 0x84, 0x0c, 0x25, 0x05, 0x08, 0x19, 0x1e, 0x04, 0x8b, 0x62, 0x80, 0x10, 0x45, 0x02,  // ....%......b..E.
    0x42, 0x92, 0x0b, 0x42, 0x84, 0x10, 0x32, 0x14, 0x38, 0x08, 0x18, 0x4b, 0x0a, 0x32, 0x42, 0x88,  // B..B..2.8..K.2B.
    0x48, 0x90, 0x14, 0x20, 0x43, 0x46, 0x88, 0xa5, 0x00, 0x19, 0x32, 0x42, 0xe4, 0x48, 0x0e, 0x90,  // H.. CF....2B.H..
    0x11, 0x22, 0xc4, 0x50, 0x41, 0x51, 0x81, 0x8c, 0xe1, 0x83, 0xe5, 0x8a, 0x04, 0x21, 0x46, 0x06,  // .".PAQ.......!F.
    0x51, 0x18, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1b, 0x8c, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x07,  // Q...............
    0x40, 0x02, 0xa8, 0x0d, 0x84, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x03, 0x20, 0x01, 0x00, 0x00, 0x00,  // @.......... ....
    0x49, 0x18, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x13, 0x82, 0x60, 0x42, 0x20, 0x00, 0x00, 0x00,  // I.........`B ...
    0x89, 0x20, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x32, 0x22, 0x08, 0x09, 0x20, 0x64, 0x85, 0x04,  // . ......2".. d..
    0x13, 0x22, 0xa4, 0x84, 0x04, 0x13, 0x22, 0xe3, 0x84, 0xa1, 0x90, 0x14, 0x12, 0x4c, 0x88, 0x8c,  // ."...."......L..
    0x0b, 0x84, 0x84, 0x4c, 0x10, 0x30, 0x23, 0x00, 0x25, 0x00, 0x8a, 0x19, 0x80, 0x39, 0x02, 0x30,  // ...L.0#.%....9.0
    0x98, 0x23, 0x40, 0x8a, 0x31, 0x44, 0x54, 0x44, 0x56, 0x0c, 0x20, 0xa2, 0x1a, 0xc2, 0x81, 0x80,  // .#@.1DTDV. .....
    0x54, 0x20, 0x00, 0x00, 0x13, 0x14, 0x72, 0xc0, 0x87, 0x74, 0x60, 0x87, 0x36, 0x68, 0x87, 0x79,  // T ....r..t`.6h.y
    0x68, 0x03, 0x72, 0xc0, 0x87, 0x0d, 0xaf, 0x50, 0x0e, 0x6d, 0xd0, 0x0e, 0x7a, 0x50, 0x0e, 0x6d,  // h.r....P.m..zP.m
    0x00, 0x0f, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0xa0,  // ..z0.r..s .m..q.
    0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x78, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e,  // .s .m..x..s .m..
    0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe9, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x73,  // q`.z0.r...0.r..s
    0x20, 0x07, 0x6d, 0x90, 0x0e, 0x76, 0x40, 0x07, 0x7a, 0x60, 0x07, 0x74, 0xd0, 0x06, 0xe6, 0x10,  //  .m..v@.z`.t....
    0x07, 0x76, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x60, 0x0e, 0x73, 0x20, 0x07, 0x7a, 0x30, 0x07,  // .v..s .m`.s .z0.
    0x72, 0xd0, 0x06, 0xe6, 0x60, 0x07, 0x74, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x6d, 0xe0, 0x0e, 0x78,  // r...`.t..v@.m..x
    0xa0, 0x07, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x43, 0x9e,  // ..q`.z0.r..v@.C.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x3c, 0x06, 0x10, 0x00,  // ............<...
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x79, 0x10, 0x20, 0x00, 0x04, 0x00, 0x00,  // .........y. ....
    0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x02, 0x01, 0x0c, 0x00, 0x00, 0x00, 0x32, 0x1e, 0x98, 0x14,  // ............2...
    0x19, 0x11, 0x4c, 0x90, 0x8c, 0x09, 0x26, 0x47, 0xc6, 0x04, 0x43, 0xa2, 0x12, 0x18, 0x01, 0x28,  // ..L...&G..C....(
    0x87, 0x62, 0x28, 0x83, 0xf2, 0xa0, 0x2a, 0x89, 0x32, 0x28, 0x84, 0x11, 0x80, 0x22, 0x28, 0x10,  // .b(...*.2(..."(.
    0xda, 0xb1, 0x14, 0x84, 0x00, 0x00, 0x00, 0x08, 0x04, 0x02, 0x01, 0x00, 0x79, 0x18, 0x00, 0x00,  // ............y...
    0x4a, 0x00, 0x00, 0x00, 0x1a, 0x03, 0x4c, 0x90, 0x46, 0x02, 0x13, 0xc4, 0x31, 0x20, 0xc3, 0x1b,  // J.....L.F...1 ..
    0x43, 0x81, 0x93, 0x4b, 0xb3, 0x0b, 0xa3, 0x2b, 0x4b, 0x01, 0x89, 0x71, 0xb9, 0x71, 0x81, 0x71,  // C..K...+K..q.q.q
    0x99, 0xc9, 0x99, 0xc9, 0x01, 0x41, 0xa9, 0x29, 0x83, 0xc1, 0x81, 0x09, 0xbb, 0xb9, 0x91, 0x49,  // .....A.).......I
    0xd9, 0x10, 0x04, 0x13, 0x04, 0x62, 0x98, 0x20, 0x10, 0xc4, 0x06, 0x61, 0x20, 0x26, 0x08, 0x44,  // .....b. ...a &.D
    0xb1, 0x41, 0x18, 0x0c, 0x0a, 0x70, 0x73, 0x13, 0x04, 0xc2, 0xd8, 0x30, 0x20, 0x09, 0x31, 0x41,  // .A...ps....0 .1A
    0x58, 0x9e, 0x0d, 0xc1, 0x32, 0x41, 0x10, 0x00, 0x1a, 0x50, 0x53, 0x4d, 0x61, 0x69, 0x6e, 0x5c,  // X...2A...PSMain\
    0xa6, 0xac, 0xbe, 0xa0, 0xde, 0xe6, 0xd2, 0xe8, 0xd2, 0xde, 0xdc, 0x26, 0x08, 0x45, 0x32, 0x41,  // ...........&.E2A
    0x28, 0x94, 0x0d, 0x01, 0x31, 0x41, 0x28, 0x96, 0x09, 0x42, 0xc1, 0x6c, 0x58, 0x88, 0x07, 0x8a,  // (...1A(..B.lX...
    0xa4, 0x69, 0x98, 0x08, 0x0a, 0x60, 0x31, 0xf4, 0xc4, 0xf4, 0x24, 0x35, 0x41, 0x28, 0x9a, 0x09,  // .i...`1...$5A(..
    0x02, 0x71, 0x4c, 0x10, 0x08, 0x64, 0x83, 0x80, 0x65, 0x1b, 0x96, 0xc1, 0x82, 0x28, 0xe9, 0x1a,  // .qL..d..e....(..
    0xa6, 0x81, 0xd2, 0x36, 0x08, 0xd5, 0xc6, 0x64, 0xca, 0xea, 0x8b, 0x2a, 0x4c, 0xee, 0xac, 0x8c,  // ...6...d...*L...
    0x6e, 0x82, 0x50, 0x38, 0x1b, 0x16, 0xa2, 0x83, 0x3c, 0x89, 0x1a, 0x26, 0x82, 0xd2, 0x36, 0x04,  // n.P8....<..&..6.
    0xdf, 0x86, 0x81, 0x03, 0x03, 0x60, 0x43, 0xd1, 0x38, 0x61, 0x00, 0x00, 0x55, 0xd8, 0xd8, 0xec,  // .....`C.8a..U...
    0xda, 0x5c, 0xd2, 0xc8, 0xca, 0xdc, 0xe8, 0xa6, 0x04, 0x41, 0x15, 0x32, 0x3c, 0x17, 0xbb, 0x32,  // .\.......A.2<..2
    0xb9, 0xb9, 0xb4, 0x37, 0xb7, 0x29, 0x01, 0xd1, 0x84, 0x0c, 0xcf, 0xc5, 0x2e, 0x8c, 0xcd, 0xae,  // ...7.)..........
    0x4c, 0x6e, 0x4a, 0x60, 0xd4, 0x21, 0xc3, 0x73, 0x99, 0x43, 0x0b, 0x23, 0x2b, 0x93, 0x6b, 0x7a,  // LnJ`.!.s.C.#+.kz
    0x23, 0x2b, 0x63, 0x9b, 0x12, 0x24, 0x75, 0xc8, 0xf0, 0x5c, 0xec, 0xd2, 0xca, 0xee, 0x92, 0xc8,  // #+c..$u..\......
    0xa6, 0xe8, 0xc2, 0xe8, 0xca, 0xa6, 0x04, 0x4b, 0x1d, 0x32, 0x3c, 0x97, 0x32, 0x37, 0x3a, 0xb9,  // .......K.2<.27:.
    0x3c, 0xa8, 0xb7, 0x34, 0x37, 0xba, 0xb9, 0x29, 0x41, 0x18, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00,  // <..47..)A...y...
    0x4c, 0x00, 0x00, 0x00, 0x33, 0x08, 0x80, 0x1c, 0xc4, 0xe1, 0x1c, 0x66, 0x14, 0x01, 0x3d, 0x88,  // L...3......f..=.
    0x43, 0x38, 0x84, 0xc3, 0x8c, 0x42, 0x80, 0x07, 0x79, 0x78, 0x07, 0x73, 0x98, 0x71, 0x0c, 0xe6,  // C8...B..yx.s.q..
    0x00, 0x0f, 0xed, 0x10, 0x0e, 0xf4, 0x80, 0x0e, 0x33, 0x0c, 0x42, 0x1e, 0xc2, 0xc1, 0x1d, 0xce,  // ........3.B.....
    0xa1, 0x1c, 0x66, 0x30, 0x05, 0x3d, 0x88, 0x43, 0x38, 0x84, 0x83, 0x1b, 0xcc, 0x03, 0x3d, 0xc8,  // ..f0.=.C8.....=.
    0x43, 0x3d, 0x8c, 0x03, 0x3d, 0xcc, 0x78, 0x8c, 0x74, 0x70, 0x07, 0x7b, 0x08, 0x07, 0x79, 0x48,  // C=..=.x.tp.{..yH
    0x87, 0x70, 0x70, 0x07, 0x7a, 0x70, 0x03, 0x76, 0x78, 0x87, 0x70, 0x20, 0x87, 0x19, 0xcc, 0x11,  // .pp.zp.vx.p ....
    0x0e, 0xec, 0x90, 0x0e, 0xe1, 0x30, 0x0f, 0x6e, 0x30, 0x0f, 0xe3, 0xf0, 0x0e, 0xf0, 0x50, 0x0e,  // .....0.n0.....P.
    0x33, 0x10, 0xc4, 0x1d, 0xde, 0x21, 0x1c, 0xd8, 0x21, 0x1d, 0xc2, 0x61, 0x1e, 0x66, 0x30, 0x89,  // 3....!..!..a.f0.
    0x3b, 0xbc, 0x83, 0x3b, 0xd0, 0x43, 0x39, 0xb4, 0x03, 0x3c, 0xbc, 0x83, 0x3c, 0x84, 0x03, 0x3b,  // ;..;.C9..<..<..;
    0xcc, 0xf0, 0x14, 0x76, 0x60, 0x07, 0x7b, 0x68, 0x07, 0x37, 0x68, 0x87, 0x72, 0x68, 0x07, 0x37,  // ...v`.{h.7h.rh.7
    0x80, 0x87, 0x70, 0x90, 0x87, 0x70, 0x60, 0x07, 0x76, 0x28, 0x07, 0x76, 0xf8, 0x05, 0x76, 0x78,  // ..p..p`.v(.v..vx
    0x87, 0x77, 0x80, 0x87, 0x5f, 0x08, 0x87, 0x71, 0x18, 0x87, 0x72, 0x98, 0x87, 0x79, 0x98, 0x81,  // .w.._..q..r..y..
    0x2c, 0xee, 0xf0, 0x0e, 0xee, 0xe0, 0x0e, 0xf5, 0xc0, 0x0e, 0xec, 0x30, 0x03, 0x62, 0xc8, 0xa1,  // ,..........0.b..
    0x1c, 0xe4, 0xa1, 0x1c, 0xcc, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xdc, 0x61, 0x1c, 0xca, 0x21, 0x1c,  // ...........a..!.
    0xc4, 0x81, 0x1d, 0xca, 0x61, 0x06, 0xd6, 0x90, 0x43, 0x39, 0xc8, 0x43, 0x39, 0x98, 0x43, 0x39,  // ....a...C9.C9.C9
    0xc8, 0x43, 0x39, 0xb8, 0xc3, 0x38, 0x94, 0x43, 0x38, 0x88, 0x03, 0x3b, 0x94, 0xc3, 0x2f, 0xbc,  // .C9..8.C8..;../.
    0x83, 0x3c, 0xfc, 0x82, 0x3b, 0xd4, 0x03, 0x3b, 0xb0, 0xc3, 0x0c, 0xc4, 0x21, 0x07, 0x7c, 0x70,  // .<..;..;....!.|p
    0x03, 0x7a, 0x28, 0x87, 0x76, 0x80, 0x87, 0x19, 0xd1, 0x43, 0x0e, 0xf8, 0xe0, 0x06, 0xe4, 0x20,  // .z(.v....C..... 
    0x0e, 0xe7, 0xe0, 0x06, 0xf6, 0x10, 0x0e, 0xf2, 0xc0, 0x0e, 0xe1, 0x90, 0x0f, 0xef, 0x50, 0x0f,  // ..............P.
    0xf4, 0x00, 0x00, 0x00, 0x71, 0x20, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06, 0x60, 0xa4, 0xac,  // ....q .......`..
    0x09, 0x20, 0x8d, 0x05, 0x4c, 0xc3, 0xe5, 0x3b, 0x8f, 0xbf, 0x38, 0xc0, 0x20, 0x36, 0x0f, 0x35,  // . ..L..;..8. 6.5
    0xf9, 0xc5, 0x6d, 0x9b, 0x40, 0x35, 0x5c, 0xbe, 0xf3, 0xf8, 0xd2, 0xe4, 0x44, 0x04, 0x4a, 0x4d,  // ..m.@5\.....D.JM
    0x0f, 0x35, 0xf9, 0xc5, 0x6d, 0x03, 0x00, 0x00, 0x61, 0x20, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,  // .5..m...a ......
    0x13, 0x04, 0x41, 0x2c, 0x10, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x44, 0x85, 0x30, 0x03,  // ..A,........D.0.
    0x50, 0x0a, 0x54, 0x25, 0x00, 0x00, 0x00, 0x00, 0x23, 0x06, 0x09, 0x00, 0x82, 0x60, 0x60, 0x4c,  // P.T%....#....``L
    0x44, 0x04, 0x21, 0xc3, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x18, 0x54, 0x21, 0x45, 0x02, 0x31,  // D.!..A.. ..T!E.1
    0x62, 0x90, 0x00, 0x20, 0x08, 0x06, 0x46, 0x65, 0x4c, 0x52, 0x52, 0x8c, 0x18, 0x24, 0x00, 0x08,  // b.. ..FeLRR..$..
    0x82, 0x81, 0x61, 0x1d, 0xd4, 0xd4, 0x18, 0x23, 0x06, 0x09, 0x00, 0x82, 0x60, 0x80, 0x58, 0x06,  // ..a....#....`.X.
    0x45, 0x31, 0xc4, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x20, 0x96, 0x41, 0x51, 0xc5, 0x30, 0x62,  // E1..A.. . .AQ.0b
    0x90, 0x00, 0x20, 0x08, 0x06, 0x88, 0x65, 0x50, 0xd4, 0x22, 0x8c, 0x18, 0x24, 0x00, 0x08, 0x82,  // .. ...eP."..$...
    0x01, 0x62, 0x19, 0x14, 0xe5, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 // .b..........
};
