using System;

namespace Nanoforge
{
	typealias u8 = uint8;
	typealias u16 = uint16;
	typealias u32 = uint32;
	typealias u64 = uint64;

	typealias i8 = int8;
	typealias i16 = int16;
	typealias i32 = int32;
	typealias i64 = int64;

	typealias f32 = float;
	typealias f64 = double;
}

static
{
    //Ensure sized types are the expected size. If any of these fail you're either on a weird platform or there are serious problems
    [Comptime]
	static void ValidateNanoforgeTypeSizes()
    {
        Compiler.Assert(sizeof(Nanoforge.u8) == 1);
        Compiler.Assert(sizeof(Nanoforge.u16) == 2);
        Compiler.Assert(sizeof(Nanoforge.u32) == 4);
        Compiler.Assert(sizeof(Nanoforge.u64) == 8);

        Compiler.Assert(sizeof(Nanoforge.i8) == 1);
        Compiler.Assert(sizeof(Nanoforge.i16) == 2);
        Compiler.Assert(sizeof(Nanoforge.i32) == 4);
        Compiler.Assert(sizeof(Nanoforge.i64) == 8);

        Compiler.Assert(sizeof(Nanoforge.f32) == 4);
        Compiler.Assert(sizeof(Nanoforge.f64) == 8);
    }
}
