#include "TestUtils.hpp"
#include <algorithm>
#include <memory>



namespace EnumerableTests {

	namespace AllocatorInternals {

		/// Untyped control block for TestAllocator
		template <unsigned MaxCount>
		struct BufferControl {
			size_t		free;
			unsigned	refCount		   = 1;
			unsigned	count			   = 0;
			void*		pointers[MaxCount] = { nullptr };

		
			BufferControl(size_t space) : free { space }	// for old MSVC's placement-new
			{
			}


			bool HasFreedAll() const noexcept
			{
				bool any = false;
				for (void* p : pointers)
					any |= p != nullptr;

				return !any;
			}


			void AlignAndBook(void*& trg, size_t alignReq, const size_t sizeReq)
			{
				bool canBook = count < MaxCount;
				bool fit	 = std::align(alignReq, sizeReq, trg, free);

				if (fit && canBook) {
					free			 -= sizeReq;
					pointers[count++] = trg;
					return;
				}

				Assert(fit,		"Testallocator buffer depleted.",		__FILE__, __LINE__);
				Assert(canBook, "Testallocator object count exceeded.",	__FILE__, __LINE__);
				throw std::bad_alloc();
			}


			void Unbook(void* obj) noexcept
			{
				ASSERT (count > 0);

				for (unsigned i = 0; i < count; i++) {
					if (obj == pointers[i]) {
						pointers[i]		= pointers[--count];
						pointers[count]	= nullptr;

						ASSERT (count > 0 || HasFreedAll());
						return;
					}
				}

				Assert(false, "Bad deallocation!",	__FILE__, __LINE__);
			}
		};

	}



	/// A simplified allocator with deallocation checks over a single fixed buffer.
	/// Denies requests after depleted. Reusable once everything is properly freed.
	/// @tparam MaxCount: count of allowed allocations - constrains bookkeeping space.
	template <class T, unsigned MaxCount>
	class TestAllocator {
		using BufferControl = AllocatorInternals::BufferControl<MaxCount>;

		char* 			buffer;
		const size_t	size;
		const size_t	objSpace;
		BufferControl&	control;


		static_assert (alignof(BufferControl) <= alignof(size_t), "Unexpected alignment.");

		// at the end of buffer
		char* CalcControlAddr() const
		{
			char* const p = buffer + size - sizeof(BufferControl);

			ASSERT_EQ (0, reinterpret_cast<uintptr_t>(p) % alignof(BufferControl));

			return p;
		}


		char*  NextFreeByte() const noexcept
		{
			return buffer + (objSpace - control.free);
		}


	public:
		static constexpr size_t   alignReq = std::max(alignof(size_t), alignof(T));


		size_t BytesRemaining() const noexcept
		{
			return control.free;
		}


		template<size_t C>
		TestAllocator(std::aligned_storage_t<sizeof(T), alignReq> (&storage)[C]) :
			buffer   { reinterpret_cast<char*>(storage) },
			size     { C * sizeof(T) },
			objSpace { size - sizeof(BufferControl) },
			control  { *new (CalcControlAddr()) BufferControl { objSpace } }
		{
			// friendly reminder - can't account for possible rebind<U>
			static_assert (C * sizeof(T) >= MaxCount * sizeof(T) + sizeof(BufferControl), "Buffer too small!");
		}


		// NOTE: There should be no separate move ctor to adhere standard! (Can crash tests.)
		TestAllocator(const TestAllocator& src) noexcept :
			buffer   { src.buffer },
			size     { src.size },
			objSpace { src.objSpace },
			control  { src.control }
		{
			control.refCount++;
		}


		~TestAllocator()
		{
			if (buffer && --control.refCount == 0) {
				Assert(control.HasFreedAll(), "Unfreed memory!", __FILE__, __LINE__);
				control.~BufferControl();
			}
		}

		
		TestAllocator& operator =(const TestAllocator&) = delete;



	#pragma region STL Allocator

		using value_type	= T;
		using reference		= T&;
		using pointer		= T*;
		using const_pointer = const T*;
		using size_type		  = size_t;
		using difference_type = ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal						 = std::false_type;

		
		template <class, unsigned> friend class TestAllocator;

		template <class U>
		struct rebind	{ using other = TestAllocator<U, MaxCount>; };


		template <class U>
		TestAllocator(const TestAllocator<U, MaxCount>& src) noexcept :
			buffer   { src.buffer },
			size     { src.size },
			objSpace { src.objSpace },
			control  { src.control }
		{
			control.refCount++;
		}


		bool operator ==(const TestAllocator& rhs) const noexcept
		{
			return buffer == rhs.buffer;
		}


		bool operator !=(const TestAllocator& rhs) const noexcept
		{
			return buffer != rhs.buffer;
		}


		pointer address(T& obj) const noexcept
		{
			return std::addressof(obj);
		}


		pointer allocate(size_t n)
		{
			ASSERT (n > 0);

			size_t req = n * sizeof(T);
			void*  trg = NextFreeByte();
			
			control.AlignAndBook(trg, alignof(T), req);
			return static_cast<pointer>(trg);
		}


		void deallocate(pointer p, size_t n) noexcept
		{
			ASSERT (n > 0);

			if (p == nullptr)
				return;

			control.Unbook(p);
			
			// only supported way of reuse: if everything can be reclaimed
			if (control.count == 0)
				control.free = objSpace;
		}

	#pragma endregion

	};

}
