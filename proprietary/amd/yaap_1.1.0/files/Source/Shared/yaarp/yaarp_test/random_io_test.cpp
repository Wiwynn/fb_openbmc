/* Copyright (C) 2012, ADVANCED MICRO DEVICES, INC.  All Rights Reserved.
 * AMD Confidential Proprietary
 *
 * AMD is granting you permission to use this software (the Materials)
 * pursuant to the terms and conditions of your Software License Agreement
 * with AMD.  This header does *NOT* give you permission to use the Materials
 * or any rights under AMD's intellectual property.  Your use of any portion
 * of these Materials shall constitute your acceptance of those terms and
 * conditions.  If you do not agree to the terms and conditions of the Software
 * License Agreement, please do not use any portion of these Materials.
 *
 * CONFIDENTIALITY:  The Materials and all other information, identified as
 * confidential and provided to you by AMD shall be kept confidential in
 * accordance with the terms and conditions of the Software License Agreement.
 *
 * LIMITATION OF LIABILITY: THE MATERIALS AND ANY OTHER RELATED INFORMATION
 * PROVIDED TO YOU BY AMD ARE PROVIDED "AS IS" WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY OF ANY KIND, INCLUDING BUT NOT LIMITED TO WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, TITLE, FITNESS FOR ANY PARTICULAR PURPOSE,
 * OR WARRANTIES ARISING FROM CONDUCT, COURSE OF DEALING, OR USAGE OF TRADE.
 * IN NO EVENT SHALL AMD OR ITS LICENSORS BE LIABLE FOR ANY DAMAGES WHATSOEVER
 * (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS, BUSINESS
 * INTERRUPTION, OR LOSS OF INFORMATION) ARISING OUT OF AMD'S NEGLIGENCE,
 * GROSS NEGLIGENCE, THE USE OF OR INABILITY TO USE THE MATERIALS OR ANY OTHER
 * RELATED INFORMATION PROVIDED TO YOU BY AMD, EVEN IF AMD HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.  BECAUSE SOME JURISDICTIONS PROHIBIT THE
 * EXCLUSION OR LIMITATION OF LIABILITY FOR CONSEQUENTIAL OR INCIDENTAL DAMAGES,
 * THE ABOVE LIMITATION MAY NOT APPLY TO YOU.
 *
 * AMD does not assume any responsibility for any errors which may appear in
 * the Materials or any other related information provided to you by AMD, or
 * result from use of the Materials or any related information.
 *
 * You agree that you will not reverse engineer or decompile the Materials.
 *
 * NO SUPPORT OBLIGATION: AMD is not obligated to furnish, support, or make any
 * further information, software, technical information, know-how, or show-how
 * available to you.  Additionally, AMD retains the right to modify the
 * Materials at any time, without notice, and is not obligated to provide such
 * modified Materials to you.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS: The Materials are provided with
 * "RESTRICTED RIGHTS." Use, duplication, or disclosure by the Government is
 * subject to the restrictions as set forth in FAR 52.227-14 and
 * DFAR252.227-7013, et seq., or its successor.  Use of the Materials by the
 * Government constitutes acknowledgement of AMD's proprietary rights in them.
 *
 * EXPORT ASSURANCE:  You agree and certify that neither the Materials, nor any
 * direct product thereof will be exported directly or indirectly, into any
 * country prohibited by the United States Export Administration Act and the
 * regulations thereunder, without the required authorization from the U.S.
 * government nor will be used for any purpose prohibited by the same.
 */

#include <boost/random.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include <gtest/gtest.h>

#include <yaarp/output_stream.h>
#include <yaarp/input_stream.h>

#include <inttypes.h>

namespace YAARP {
	namespace IOTests {
		/**
		 * This class keeps track of where we are in the OutputStream and keeps
		 * an InputStream which points to the current OutputStream buflet.
		 */
		class IOIterator {
		public:
			/**
			 * Constructor; initializes us to the beginning of the outputStream.
			 */
			IOIterator( OutputStream & outputStream ) {
				m_buflet = outputStream.begin();
				m_inputStream = InputStreamPtr(new InputStream(m_buflet->begin(), m_buflet->validBytes()));
			}

			/**
			 * Increments to the next output buflet, if necessary. Returns true
			 * if there is more data available, false if there is not.
			 */
			bool increment() {
				if( m_buflet == NULL ) {
					return false;
				}

				if( m_inputStream->getBytesRemaining() == 0 ) {
					m_buflet = m_buflet->getNext();

					if( m_buflet != NULL ) {
						m_inputStream = InputStreamPtr(new InputStream(m_buflet->begin(), m_buflet->validBytes()) );
					} else {
						return false;
					}
				}

				return true;
			}

			InputStream & inputStream() {
				return *m_inputStream.get();
			}

		private:
			typedef boost::shared_ptr<InputStream> InputStreamPtr;

			InputStreamPtr m_inputStream;
			OutputStream::StreamBuflet * m_buflet;
		};

		/**
		 * An abstract class which makes a single transaction with an OutputStream, and
		 * can then perform the inverse on an InputStream and validate that the result
		 * is correct.
		 *
		 * There should be an implementation of this class for each of the function 
		 * pairs in InputStream and OutputStream.
		 */
		class IOTest {
		public:
			typedef boost::shared_ptr<IOTest> ptr;

			// The list of all the possible types. If you add a new Tester 
			// type, you should add it here (before TYPE_MAX).
			// Note: we only have one enum type for the blocks, so we can 
			//       guarantee that they always happen in pairs.
			enum IOType {
				TYPE_BYTE = 0,
				TYPE_INT,
				TYPE_LONG,
				TYPE_DOUBLE,
				TYPE_BLOCK, 
				TYPE_BYTE_ARRAY,
				TYPE_INT_ARRAY,
				TYPE_LONG_ARRAY,
				TYPE_DOUBLE_ARRAY,
				TYPE_MAX
			};

			virtual ~IOTest() {}

			/**
			 * Writes a value to the stream which can be read back and 
			 * validated later.
			 */
			virtual void output( OutputStream & out ) = 0;

			/**
			 * Reads back the same value and validates that it is what was 
			 * written to the OutputStream.
			 */
			virtual void inputAndValidate( IOIterator & in ) = 0;
		};

		/// Tests putting and getting a single byte.
		class ByteTest : public IOTest {
		public:
			ByteTest( uint8_t value ) : m_value(value) {}

			void output( OutputStream & out ) {
				out.putByte(m_value);
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());
				ASSERT_EQ(m_value,in.inputStream().getByte());
			}
		private:
			uint8_t m_value;
		};

		/// Tests putting and getting a 4-byte integer.
		class IntTest : public IOTest {
		public:
			IntTest( uint32_t value ) : m_value(value) {}

			void output( OutputStream & out ) {
				out.putInt(m_value);
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());
				ASSERT_EQ(m_value,in.inputStream().getInt());
			}
		private:
			uint32_t m_value;
		};

		/// Tests putting and getting an 8-byte integer.
		class LongTest : public IOTest {
		public:
			LongTest( uint64_t value ) : m_value(value) {}

			void output( OutputStream & out ) {
				out.putLong(m_value);
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());
				ASSERT_EQ(m_value,in.inputStream().getLong());
			}
		private:
			uint64_t m_value;
		};

		/// Tests putting and getting a double
		class DoubleTest : public IOTest {
		public:
			DoubleTest( double value ) : m_value(value) {}

			void output( OutputStream & out ) {
				out.putDouble(m_value);
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());
				ASSERT_DOUBLE_EQ(m_value,in.inputStream().getDouble());
			}
		private:
			double m_value;
		};

		class BeginBlockTest : public IOTest {
		public:
			typedef boost::shared_ptr<BeginBlockTest> ptr;
			BeginBlockTest() : m_size(0) {}
			
			void output( OutputStream & out ) {
				out.beginBlockSize();
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());
				ASSERT_EQ(m_size,in.inputStream().getInt());
			}

			void setSize( uint32_t size ) {
				m_size = size;
			}
		private:
			uint32_t m_size;
		};

		class EndBlockTest : public IOTest {
		public:
			void output( OutputStream & out ) {
				out.endBlockSize();
			}

			// no-op
			void inputAndValidate( IOIterator & ) {
			}
		};

		/// Tests putting and getting a byte array
		class ByteArrayTest : public IOTest {
		public:
			typedef boost::shared_array<uint8_t> Data;

			ByteArrayTest( Data data, size_t length ) 
				: m_data(data), m_length(length) {}

			void output( OutputStream & out ) {
				out.putByteArray(m_length, m_data.get());
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());

				if( in.inputStream().getBytesRemaining() >= m_length + 4 ) {
					Data actual = Data(new uint8_t[m_length]);

					size_t len = in.inputStream().getArrayCount();
					ASSERT_EQ(len, m_length);
					in.inputStream().getByteArray(m_length,actual.get());

					for( size_t i = 0; i < m_length; i++ ) {
						ASSERT_EQ(static_cast<uint32_t>(m_data[i]),
							static_cast<uint32_t>(actual[i])) << " at index " << i;
					}
				} else {
					size_t actualLength = in.inputStream().getArrayCount();

					ASSERT_EQ(m_length,actualLength);

					for( size_t i = 0; i < m_length; i++ ) {
						ASSERT_TRUE(in.increment());
						ASSERT_EQ(static_cast<uint32_t>(m_data[i]),
							static_cast<uint32_t>(in.inputStream().getByte()))
							 << " at index " << i;
					}
				}
			}
		private:
			Data m_data;
			size_t m_length;
		};

		class IntArrayTest : public IOTest {
		public:
			typedef boost::shared_array<uint32_t> Data;

			IntArrayTest( Data data, size_t length ) 
				: m_data(data), m_length(length) {}

			void output( OutputStream & out ) {
				out.putIntArray(m_length, m_data.get());
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());

				if( in.inputStream().getBytesRemaining() >= (m_length*4) + 4 ) {
					Data actual = Data(new uint32_t[m_length]);

					size_t len = in.inputStream().getArrayCount();
					ASSERT_EQ(len, m_length);
					in.inputStream().getIntArray(m_length,actual.get());

					for( size_t i = 0; i < m_length; i++ ) {
						ASSERT_EQ(m_data[i], actual[i]) << " at index " << i;
					}
				} else {
					size_t actualLength = in.inputStream().getArrayCount();

					ASSERT_EQ(m_length,actualLength);

					for( size_t i = 0; i < m_length; i++ ) {
						ASSERT_TRUE(in.increment());
						ASSERT_EQ(m_data[i], in.inputStream().getInt())
							 << " at index " << i;
					}
				}
			}
		private:
			Data m_data;
			size_t m_length;
		};

		class LongArrayTest : public IOTest {
		public:
			typedef boost::shared_array<uint64_t> Data;

			LongArrayTest( Data data, size_t length ) 
				: m_data(data), m_length(length) {}

			void output( OutputStream & out ) {
				out.putLongArray(m_length, m_data.get());
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());

				if( in.inputStream().getBytesRemaining() >= (m_length*8) + 4 ) {
					Data actual = Data(new uint64_t[m_length]);

					size_t len = in.inputStream().getArrayCount();
					ASSERT_EQ(len, m_length);
					in.inputStream().getLongArray(m_length,actual.get());

					for( size_t i = 0; i < m_length; i++ ) {
						ASSERT_EQ(m_data[i], actual[i]) << " at index " << i;
					}
				} else {
					size_t actualLength = in.inputStream().getArrayCount();

					ASSERT_EQ(m_length,actualLength);

					for( size_t i = 0; i < m_length; i++ ) {
						ASSERT_TRUE(in.increment());
						ASSERT_EQ(m_data[i], in.inputStream().getLong())
							 << " at index " << i;
					}
				}
			}
		private:
			Data m_data;
			size_t m_length;
		};

		class DoubleArrayTest : public IOTest {
		public:
			typedef boost::shared_array<double> Data;

			DoubleArrayTest( Data data, size_t length ) 
				: m_data(data), m_length(length) {}

			void output( OutputStream & out ) {
				out.putDoubleArray(m_length, m_data.get());
			}

			void inputAndValidate( IOIterator & in ) {
				ASSERT_TRUE(in.increment());

				if( in.inputStream().getBytesRemaining() >= (m_length*8) + 4 ) {
					Data actual = Data(new double[m_length]);

					size_t len = in.inputStream().getArrayCount();
					ASSERT_EQ(len, m_length);
					in.inputStream().getDoubleArray(m_length,actual.get());

					for( size_t i = 0; i < m_length; i++ ) {
						ASSERT_DOUBLE_EQ(m_data[i], actual[i]) << " at index " << i;
					}
				} else {
					size_t actualLength = in.inputStream().getArrayCount();

					ASSERT_EQ(m_length,actualLength);

					for( size_t i = 0; i < m_length; i++ ) {
						ASSERT_TRUE(in.increment());
						ASSERT_DOUBLE_EQ(m_data[i], in.inputStream().getDouble())
							 << " at index " << i;
					}
				}
			}
		private:
			Data m_data;
			size_t m_length;
		};

		/// A list of tests to execute.
		typedef std::vector<IOTest::ptr> TestList;

		/**
		 * Generates a random list of tests.
		 *
		 * Logs the seed being used, so the test can be reproduced with the 
		 * same seed value, to regress any bugs which are found.
		 */
		void generateRandomTests( TestList & tests, boost::uint32_t seed ) {
			::testing::Test::RecordProperty("seed",seed);
			
			typedef boost::mt19937 randomness_type;
	
			typedef boost::uniform_smallint<int> type_distribution;
			typedef boost::uniform_smallint<int> byte_distribution;
			typedef boost::uniform_int<uint32_t> int_distribution;
			typedef boost::uniform_int<uint64_t> long_distribution;
			typedef boost::uniform_real<double> double_distribution;

			typedef boost::variate_generator<randomness_type, type_distribution> type_generator;
			typedef boost::variate_generator<randomness_type, byte_distribution> byte_generator;
			typedef boost::variate_generator<randomness_type, int_distribution> int_generator;
			typedef boost::variate_generator<randomness_type, long_distribution> long_generator;
			typedef boost::variate_generator<randomness_type, double_distribution> double_generator;

			randomness_type randomness(seed);

			type_generator randomType( randomness, type_distribution(0,IOTest::TYPE_MAX-1) );
			byte_generator randomByte( randomness, byte_distribution(0,0xFF) );
			int_generator randomInt( randomness, int_distribution(0,0xFFFFFFFF) );
			long_generator randomLong( randomness, long_distribution(0,0xFFFFFFFFFFFFFFFF) );
			double_generator randomDouble( randomness, double_distribution(std::numeric_limits<double>::min(), std::numeric_limits<double>::max()) );

			int_generator randomCount( randomness, int_distribution(10,1000) );
			int_generator randomByteArrayCount( randomness, int_distribution(0,5000) );
			int_generator randomIntArrayCount( randomness, int_distribution(0,2000) );
			int_generator randomLongArrayCount( randomness, int_distribution(0,1000) );

			size_t count = randomCount();

			BeginBlockTest::ptr beginBlock;
			uint32_t blockSize = 0;
			uint32_t blockEnd = 0;

			for( size_t i = 0; i < count; i++ ) {
				IOTest::ptr test;

				// If we've reached the end of a block, insert that, rather 
				// than a randomly-generated test.
				if( beginBlock && i == blockEnd ) {
					test = IOTest::ptr( new EndBlockTest() );
					beginBlock->setSize(blockSize);
					beginBlock = BeginBlockTest::ptr();
					blockEnd = 0;
				} else {
					IOTest::IOType type = static_cast<IOTest::IOType>(randomType());
					switch( type ) {
						case IOTest::TYPE_BYTE:
							test = IOTest::ptr( new ByteTest(static_cast<uint8_t>(randomByte())) );
							if( beginBlock ) {
								blockSize += 1;
							}
							break;
						case IOTest::TYPE_INT:
							test = IOTest::ptr( new IntTest(randomInt()) );
							if( beginBlock ) {
								blockSize += 4;
							}
							break;
						case IOTest::TYPE_LONG:
							test = IOTest::ptr( new LongTest(randomLong()) );
							if( beginBlock ) {
								blockSize += 8;
							}
							break;
						case IOTest::TYPE_DOUBLE:
							test = IOTest::ptr( new DoubleTest(randomDouble()) );
							if( beginBlock ) {
								blockSize += 8;
							}
							break;
						case IOTest::TYPE_BLOCK:
							// We can't generate a begin block if we're in the
							// middle of a block, or if there isn't room in the
							// list for an end block.
							if( !beginBlock && i < count-1 ) {
								test = IOTest::ptr( new BeginBlockTest() );
								beginBlock = boost::shared_dynamic_cast<BeginBlockTest>(test);
								blockSize = 0;

								blockEnd = int_generator( randomness, int_distribution(i+1,count-1) )();
							} else {
								continue;
							}
							break;
						case IOTest::TYPE_BYTE_ARRAY:
							{
								size_t arrayCount = randomByteArrayCount();
								ByteArrayTest::Data data;
								if( arrayCount > 0 ) {
									 data = ByteArrayTest::Data( new uint8_t[arrayCount] );

									for( size_t i = 0; i < arrayCount; i++ ) {
										data[i] = randomByte();
									}
								}
								test = IOTest::ptr( new ByteArrayTest(data,arrayCount) );
								if( beginBlock ) {
									blockSize += 4 + arrayCount;
								}
							}
							break;
						case IOTest::TYPE_INT_ARRAY:
							{
								size_t arrayCount = randomIntArrayCount();
								IntArrayTest::Data data;
								if( arrayCount > 0 ) {
									 data = IntArrayTest::Data( new uint32_t[arrayCount] );

									for( size_t i = 0; i < arrayCount; i++ ) {
										data[i] = randomInt();
									}
								}
								test = IOTest::ptr( new IntArrayTest(data,arrayCount) );
								if( beginBlock ) {
									blockSize += 4 + (arrayCount*4);
								}
							}
							break;
						case IOTest::TYPE_LONG_ARRAY:
							{
								size_t arrayCount = randomLongArrayCount();
								LongArrayTest::Data data;
								if( arrayCount > 0 ) {
									 data = LongArrayTest::Data( new uint64_t[arrayCount] );

									for( size_t i = 0; i < arrayCount; i++ ) {
										data[i] = randomLong();
									}
								}
								test = IOTest::ptr( new LongArrayTest(data,arrayCount) );
								if( beginBlock ) {
									blockSize += 4 + (arrayCount*8);
								}
							}
							break;
						case IOTest::TYPE_DOUBLE_ARRAY:
							{
								size_t arrayCount = randomLongArrayCount();
								DoubleArrayTest::Data data;
								if( arrayCount > 0 ) {
									 data = DoubleArrayTest::Data( new double[arrayCount] );

									for( size_t i = 0; i < arrayCount; i++ ) {
										data[i] = randomDouble();
									}
								}
								test = IOTest::ptr( new DoubleArrayTest(data,arrayCount) );
								if( beginBlock ) {
									blockSize += 4 + (arrayCount*8);
								}
							}
							break;
					}
				}

				tests.push_back(test);
			}
		}

		/// Executes a list of tests.
		void executeTests( TestList & tests ) {
			OutputStream outputStream;

			int i = 0;

			// First, iterate over the list of tests and push their values into
			// the OutputStream.
			for( TestList::iterator testOut = tests.begin();
				testOut != tests.end();
				++testOut ) 
			{
				(*testOut)->output(outputStream);
				i++;
			}

			// Now we need to use InputStream to read the values back and 
			// validate that the value we read is the same as the one we wrote.
			IOIterator ioIter( outputStream );

			i = 0;

			for( TestList::iterator testIn = tests.begin();
				testIn != tests.end();
				++testIn )
			{
				ASSERT_NO_FATAL_FAILURE((*testIn)->inputAndValidate(ioIter))
					<< " on test " << testIn - tests.begin();
				i++;
			}

			// Fail if there is still data left in the output stream.
			ASSERT_FALSE(ioIter.increment()) 
				<< "More data in OutputStream than test list specifies.";
		} // executeTests()

		// This exposes a bug in the executeTests logic.
		TEST(DirectedIOTesting,EmptyBlockAtEnd) {
			TestList tests;

			tests.push_back(IOTest::ptr(new IntTest(0)));
			BeginBlockTest::ptr begin = BeginBlockTest::ptr(new BeginBlockTest());
			begin->setSize(0);
			tests.push_back(begin);
			tests.push_back(IOTest::ptr(new EndBlockTest()));

			ASSERT_NO_FATAL_FAILURE(executeTests(tests));
		}
		
		TEST(RandomIOTesting,RandomTest1) {
			TestList tests;

			uint32_t seed = 1;

			generateRandomTests(tests,seed);

			ASSERT_NO_FATAL_FAILURE(executeTests(tests));
		}

		TEST(RandomIOTesting,RandomTest2) {
			TestList tests;

			uint32_t seed = 109874;

			generateRandomTests(tests,seed);

			ASSERT_NO_FATAL_FAILURE(executeTests(tests));
		}

		TEST(RandomIOTesting,RandomTest3) {
			TestList tests;

			uint32_t seed = 43907;

			generateRandomTests(tests,seed);

			ASSERT_NO_FATAL_FAILURE(executeTests(tests));
		}

		TEST(RandomIOTesting,RandomTest4) {
			TestList tests;

			uint32_t seed = 2350123498;

			generateRandomTests(tests,seed);

			ASSERT_NO_FATAL_FAILURE(executeTests(tests));
		}

		TEST(RandomIOTesting,RandomTest5) {
			TestList tests;

			uint32_t seed = 76;

			generateRandomTests(tests,seed);

			ASSERT_NO_FATAL_FAILURE(executeTests(tests));
		}

		TEST(RandomIOTesting,ManyRandomTests) {
			for( int i = 0; i < 100; i++ ) {
				TestList tests;

				generateRandomTests(tests,i);

				ASSERT_NO_FATAL_FAILURE(executeTests(tests))
					<< " on test number " << i;
			}
		}
	} // namespace StreamTest
} // namespace YAARP
