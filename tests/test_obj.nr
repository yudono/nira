department = {
    name: "Head Office"
    sub_departments: [
        { name: "Sales", staff: 10 },
        { name: "Engineering", staff: 50 }
    ]
}

head_office = department
print(head_office.name)
print(head_office.sub_departments.length)
print(head_office.sub_departments[0].name)
