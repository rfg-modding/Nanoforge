using Common;
using System;

namespace Nanoforge.Misc
{
    //Logging interface. Writes logs to a file and keeps a certain amount of them in memory in a RingBuffer for the logging gui
	public static class Log
	{
        //TODO: Fully implement logger so it actual writes to files instead of just using Console.WriteLine. Current code is just a skeleton so I can add log messages to code before logging is fully implemented.

        public static void Info(StringView fmt, params Object[] args)
        {
        	String str = scope String(256);
        	str.AppendF(fmt, params args);
        	Console.WriteLine(str);
        }

        public static void Warning(StringView fmt, params Object[] args)
        {
        	String str = scope String(256);
        	str.AppendF(fmt, params args);
        	Console.WriteLine(str);
        }

        public static void Error(StringView fmt, params Object[] args)
        {
        	String str = scope String(256);
        	str.AppendF(fmt, params args);
        	Console.WriteLine(str);
        }
	}
}