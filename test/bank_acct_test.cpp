#include "Await.hpp"
#include "ThreadExecutor.hpp"

using namespace as;

class bank_account
{
	int balance_ = 0;
public:
	ThreadExecutor& ex_;

public:
	explicit bank_account(ThreadExecutor& ex)
		: ex_(ex)
	{
	}

	auto deposit(int amount)
	{
		return async(ex_, [=]
		             {
			             balance_ += amount;
		             });
	}

	auto withdraw(int amount)
	{
		return async(ex_, [=]
		                {
			                if (balance_ >= amount)
				                balance_ -= amount;
		                });
	}

	auto balance() const
	{
		return async(ex_, [=]
		             {
			             return balance_;
		             });
	}

	auto transfer(int amount, bank_account& to_acct)
	{
		return async(ex_, [=]
		             {
			             if (balance_ >= amount)
			             {
				             balance_ -= amount;
				             return amount;
			             }

			             return 0;
		             },

		             // [&to_acct](int deducted)
		             // {
			           //   to_acct.balance_ += deducted;
		             // },
		             [&to_acct](int deducted) {
			             return to_acct.deposit(deducted);
		             }
		            );
	}
};

template <class Iterator>
auto find_largest_account(Iterator begin, Iterator end)
{
	return await(
		[i = begin, end, largest_acct = end, balance = int(), largest_balance = int()]
		() mutable
		{
			for (; i != end; ++i)
			{
				auto fut = i->balance();
				AWAIT( fut );

				balance = fut.get();

				if (largest_acct == end || balance > largest_balance)
				{
					largest_acct = i;
					largest_balance = balance;
				}
			}

			return largest_acct;
		} );
}

int main()
{
	ThreadExecutor ex;
	std::vector<bank_account> accts(3, bank_account(ex));
	accts[0].deposit(20).get();
	accts[1].deposit(30).get();
	accts[2].deposit(40).get();
	accts[0].withdraw(10).get();
	accts[1].transfer(5, accts[0]).get().get();
	accts[2].transfer(15, accts[1]).get().get();
	std::cout << "Account 0 balance = " << accts[0].balance().get() << "\n";
	std::cout << "Account 1 balance = " << accts[1].balance().get() << "\n";
	std::cout << "Account 2 balance = " << accts[2].balance().get() << "\n";
	auto largest = find_largest_account(accts.begin(), accts.end()).get();
	std::cout << "Largest balance = " << largest->balance().get() << "\n";
}
