using System;
using Nanoforge;
using System.Collections;
using System.Threading;

namespace Nanoforge.Misc.Containers
{
    ///Fixed size queue. If more values are pushed onto it than it can store it wraps around and overwrites the first element
    public class RingBuffer<T, N> : IEnumerator<T>
                                    where T : struct
		                            where N : const int
    {
        private const int MAX_INDEX = N - 1;
        private T[N] _data;
        private int _head = 0; //Index of the element at the front of the queue
        private int _tail = 0; //Index of the element at the back of the queue
        private append Monitor _monitor = .();

        public int Capacity => N;
        public bool Empty => (_head == _tail);

        public void Push(T value)
        {
            _monitor.Enter();
            defer _monitor.Exit();

            _data[_tail] = value;
            _tail = (_tail + 1) % N;

            if (_tail == _head)
                _head = (_head + 1) % N; //Push is gonna overwrite the oldest value in the queue. Move head up one

            _monitor.Exit();
        }

        public Result<T> Pop()
        {
            _monitor.Enter();
            defer _monitor.Exit();

            if (Empty)
                return .Err;

            T value = _data[_head];
            _head = (_head + 1) % N;
            return value;
        }

        Result<T> IEnumerator<T>.GetNext()
        {
            return Pop();
        }

        public int Size
		{
            get
			{
                if (_head >= _tail)
                {
                    return (N - _head) + _tail;
                }
                else
                {
                    return _tail - _head;
                }
            }
		}

    }
}