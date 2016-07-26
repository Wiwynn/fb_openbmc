/*
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>

//#include <asm/cache.h>
#include <asm/arch/regs-scu.h>

void flush_cache(ulong start_addr, ulong size)
{
	/* Must be implemented for all M68k processors with copy-back data cache */
}

int icache_status(void)
{
//	return *cf_icache_status;
}

int dcache_status(void)
{
//	return *cf_dcache_status;
}

void icache_enable(void)
{
//	icache_invalid();

//	*cf_icache_status = 1;

}

void icache_disable(void)
{
//	u32 temp = 0;

//	*cf_icache_status = 0;
//	icache_invalid();

}

void icache_invalid(void)
{
//	u32 temp;

}

/*
 * data cache only for ColdFire V4 such as MCF547x_8x, MCF5445x
 * the dcache will be dummy in ColdFire V2 and V3
 */
void dcache_enable(void)
{
//	dcache_invalid();
//	*cf_dcache_status = 1;

}

void dcache_disable(void)
{
//	u32 temp = 0;

//	*cf_dcache_status = 0;
//	dcache_invalid();

}

void dcache_invalid(void)
{


}

void cache_enable(void)
{
#ifdef CONFIG_AST1010_CACHE
	__raw_writel( SCU_AREA3_CACHE_EN | SCU_AREA4_CACHE_EN |
					SCU_AREA5_CACHE_EN | SCU_AREA6_CACHE_EN |
					SCU_AREA7_CACHE_EN | SCU_CACHE_EN, 
					AST_SCU_BASE + AST_SCU_CPU_CACHE_CTRL);
#endif

}

void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
#ifdef CONFIG_AST1010_CACHE
	u32 counter = start;
	while (counter <= stop)
	{
		*(volatile u_long *)(AST_LINEFLUSH_BASE + 0x00000000) = counter;
		*(volatile u_long *)(AST_LINEFLUSH_BASE + 0x00000004) = counter;
		counter += (1UL << 5);
	}
#endif

}

