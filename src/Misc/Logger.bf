using System.IO;
using System;
using Common;

namespace Nanoforge.Misc
{
    //Global logging interface
    [StaticInitAfter(typeof(DateTime))] //Must initialize first or Logger constructor has an error when using DateTime.Now
	public static class Logger
	{
        public static bool AutoFlush = true;
        private static StreamWriter _writer;
        private static FileStream _fileStream;

        public static this()
        {
#if DEBUG
            StringView logFilePath = scope $@"{Workspace.Directory}\NanoforgeDebugLog.log";
#else
            StringView logFilePath = @".\NanoforgeLog.log";
#endif
            _fileStream = new .();
            if (_fileStream.Open(logFilePath, FileMode.Append, .Write, .Read) case .Err(FileOpenError err))
            {
                Runtime.FatalError(scope $"Failed to open log file. Error: {err.ToString(.. scope .())}");
            }
            _writer = new .(_fileStream, .UTF8, 4096);

            WriteLine("\n********************");
            WriteLine("Log opened at {}\n", DateTime.Now);
        }

        public static ~this()
        {
            WriteLine("\nLog closed at {}", DateTime.Now);
            WriteLine("********************\n");
            DeleteIfSet!(_writer);
            DeleteIfSet!(_fileStream);
        }

        public static void Info(StringView fmt, params Object[] args)
        {
            LabelledWrite("info", fmt, params args);
        }

        public static void Warning(StringView fmt, params Object[] args)
        {
            LabelledWrite("warning", fmt, params args);
        }

        public static void Error(StringView fmt, params Object[] args)
        {
            LabelledWrite("error", fmt, params args);
        }

        public static void Fatal(StringView fmt, params Object[] args)
        {
            LabelledWrite("fatal", fmt, params args);
        }

        private static void LabelledWrite(StringView label, StringView fmt, params Object[] args)
        {
            String str = scope String(256);
            str.AppendF(fmt, params args);
            _writer.WriteLine(scope $"[{label}][{DateTime.Now.ToString(.. scope .())}]: {str}");
            if (AutoFlush)
                Flush();
        }

        public static void WriteLine(StringView fmt, params Object[] args)
        {
            _writer.WriteLine(fmt, params args);
            if (AutoFlush)
                Flush();
        }

        public static void Write(StringView fmt, params Object[] args)
        {
            _writer.Write(fmt, params args);
            if (AutoFlush)
	            Flush();
        }

        public static void Flush()
        {
            _writer.Flush();
        }
	}
}