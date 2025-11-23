#include<stdio.h>
#include<string.h>

int AddMonthlyIncome(int m_income, int *check_m_income, int *t_income){
	if(*check_m_income==0){
		printf("Enter your monthly income\n(Once entered, can't be changed for the whole month): ");
		scanf("%d", &m_income);
		*t_income=m_income;
		(*check_m_income)++;
		printf("\n");
	}
	else{
		printf("You have entered your monthly income. You can't edit it now.\n\n");
		return 1;
	}
}

int AddExpenses(){
	
}

int main(){
	int choice;
	int monthly_income, check_monthly_income=0;
	int total_income;
	
	printf("=====Welcome to the Budget Tracker=====\n");
	
	do{
		printf("Menu:\n");
		printf("1. Add Monthly Income\n");		
		printf("2. Add Extra Income\n");
		printf("3. Add Expenses\n");		
		printf("4. Add/Delete Expenses Entries\n");
		printf("5. View Transactions\n");
		printf("6. Monthly Summary\n");		
		printf("7. Report\n");
		printf("8. Budget Limit\n");		
		printf("9. Profile Setting\n");
		printf("10. Exit\n");
		printf("\nEnter your choice: ");
		scanf("%d", &choice);
		
		switch(choice){
			case 1:
				AddMonthlyIncome(monthly_income, &check_monthly_income, &total_income);
				break;
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
				printf("Program Exited.");
				return 1;
			default:
				printf("Invalid Input! Please enter a valid input.");
		}
	}
	while(choice!=10);

	
	return 0;	
}

