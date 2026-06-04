#!/usr/bin/env python3
"""
build_domain_corpus.py — Create domain-specific Q&A corpora for EH-G3

Generates curated Q&A pairs for:
  - Medical domain (symptoms, treatments, anatomy)
  - Legal domain (contracts, rights, procedures)
  - Technical domain (programming, systems, concepts)

Output: individual corpus files for training
"""

import os

MEDICAL_QA = """# Medical Q&A Corpus - EH-G3 Domain Training
# Focus: symptoms, treatments, anatomy, health concepts

# Anatomy & Body Systems
what is the heart the heart is a muscle that pumps blood through the body
what is the brain the brain is the control center of the nervous system
what is the liver the liver is an organ that filters toxins from blood
what are lungs lungs are organs that extract oxygen from air
what is the pancreas the pancreas produces insulin to regulate blood sugar

# Common Symptoms
what causes fever fever is caused by infection or inflammatory response
what is a headache a headache is pain in the head or upper neck region
what causes nausea nausea is discomfort in the stomach often before vomiting
what is fatigue fatigue is extreme tiredness or lack of energy
what causes dizziness dizziness is a spinning sensation or feeling faint

# Diseases & Conditions
what is diabetes diabetes is a condition where blood sugar control is impaired
what is hypertension hypertension is abnormally high blood pressure
what is arthritis arthritis is inflammation of joints causing pain and stiffness
what is asthma asthma is a respiratory condition causing difficulty breathing
what is pneumonia pneumonia is an infection that inflames lung air sacs

# Treatments & Prevention
how do you treat a cold rest fluids and over the counter medications help
how do you lower fever take fever reducing medication and apply cool compress
how do you prevent infection wash hands regularly and keep wounds clean
how do you reduce inflammation anti inflammatory medications and ice help
how do you manage pain pain medication rest and physical therapy help

# Medical Procedures
what is vaccination vaccination is injecting a pathogen to build immunity
what is surgery surgery is a medical procedure using incisions to treat
what is a blood test a blood test analyzes blood samples for diseases
what is an x ray an x ray uses radiation to take images inside the body
what is ultrasound ultrasound uses sound waves to create images inside body

# Health Concepts
what is immunity immunity is resistance to infection from immune system
what is inflammation inflammation is body response to injury or infection
what is metabolism metabolism is chemical processes converting food to energy
what is blood pressure blood pressure is force of blood pushing on vessels
what is cholesterol cholesterol is a lipid in blood related to heart disease

# Medications
what are antibiotics antibiotics are drugs that kill bacteria
what are antivirals antivirals are medications that fight viral infections
what are antihistamines antihistamines block histamine to reduce allergies
what are painkillers painkillers reduce pain signals in the body
what are sedatives sedatives are medications that cause calming effects

# Nutrition
what is protein protein is nutrient needed for tissue and muscle building
what is carbohydrate carbohydrate is nutrient that provides energy
what is fat fat is nutrient needed for hormone and cell function
what is fiber fiber is indigestible carbohydrate that aids digestion
what is vitamin vitamin is organic compound needed for body functions

# Health Habits
how do you stay healthy exercise regularly eat balanced diet sleep well
how much water should you drink typically eight cups of water per day
how often should you exercise exercise at least thirty minutes daily
how much sleep do you need adults need seven to nine hours nightly
what is stress management stress management is techniques to reduce stress
"""

LEGAL_QA = """# Legal Q&A Corpus - EH-G3 Domain Training
# Focus: contracts, rights, procedures, law concepts

# Legal Concepts
what is a contract a contract is agreement between parties with legal obligations
what is liability liability is legal responsibility for damages or harm
what is jurisdiction jurisdiction is authority of court to hear cases
what is precedent precedent is prior court decision that guides future cases
what is statute statute is law enacted by legislature

# Rights & Responsibilities
what are constitutional rights constitutional rights protect citizens from government
what is freedom of speech freedom of speech allows expression without censorship
what is right to privacy right to privacy protects personal information
what is due process due process requires fair procedure before government action
what is equal protection equal protection requires laws apply equally to all

# Criminal Law
what is a crime a crime is act violating law punishable by government
what is misdemeanor misdemeanor is minor crime punishable by fines or jail
what is felony felony is serious crime punishable by imprisonment
what is bail bail allows accused to be released pending trial
what is plea bargain plea bargain is agreement to plead guilty for lighter sentence

# Civil Law
what is a lawsuit lawsuit is dispute between parties settled by court
what is damages damages are compensation for harm or loss
what is negligence negligence is failure to exercise reasonable care
what is tort tort is wrongful act causing damage creating civil liability
what is contract breach contract breach is failure to perform agreement

# Procedures & Court
what is discovery discovery is process of exchanging evidence before trial
what is deposition deposition is recorded testimony under oath before trial
what is verdict verdict is decision by judge or jury in court case
what is appeal appeal is request to higher court to review decision
what is jury jury is group of citizens judging facts in trial

# Property Law
what is real property real property is land and structures attached to land
what is personal property personal property is movable items of value
what is deed deed is document transferring ownership of property
what is mortgage mortgage is loan secured by real estate property
what is lease lease is agreement renting property for specified period

# Family Law
what is marriage marriage is legal union between spouses
what is divorce divorce is legal dissolution of marriage
what is custody custody determines which parent raises child
what is alimony alimony is support paid by one spouse to other
what is child support child support is payments for child expenses

# Employment Law
what is employment contract employment contract defines work terms conditions
what is minimum wage minimum wage is lowest legal compensation per hour
what is overtime overtime is compensation for work beyond standard hours
what is discrimination discrimination is unfair treatment based on protected class
what is workplace harassment workplace harassment is unwelcome conduct creating hostile environment

# Intellectual Property
what is copyright copyright protects original creative works
what is patent patent protects new inventions and processes
what is trademark trademark protects brand names and logos
what is intellectual property intellectual property is creations of mind
what is infringement infringement is unauthorized use of protected work
"""

TECHNICAL_QA = """# Technical Q&A Corpus - EH-G3 Domain Training
# Focus: programming, systems, concepts, algorithms

# Programming Concepts
what is a variable variable is named storage location holding value
what is a function function is reusable block of code performing task
what is a loop loop is code repetition structure executing code multiple times
what is a condition condition is control structure executing code if true
what is a data type data type defines kind of data variable can hold

# Data Structures
what is an array array is ordered collection of elements indexed by position
what is a linked list linked list is collection where elements point to next
what is a stack stack is data structure with last in first out order
what is a queue queue is data structure with first in first out order
what is a hash table hash table uses hash function to map keys to values

# Algorithms
what is sorting sorting is arranging elements in order by comparison
what is searching searching is finding element matching criteria in collection
what is recursion recursion is function calling itself to solve subproblems
what is dynamic programming dynamic programming solves problems by breaking down
what is graph traversal graph traversal visits nodes and edges systematically

# Object Oriented Programming
what is a class class is blueprint defining object structure and behavior
what is inheritance inheritance allows classes to extend parent class
what is polymorphism polymorphism allows objects to take multiple forms
what is encapsulation encapsulation bundles data and methods together
what is abstraction abstraction hides complex details showing simple interface

# Web Development
what is html html is markup language for creating web pages
what is css css is styling language controlling appearance of elements
what is javascript javascript is scripting language adding interactivity
what is a database database is organized collection of structured data
what is an api api is interface allowing programs to communicate

# Databases
what is sql sql is language for querying and managing databases
what is a query query is request for data from database
what is an index index is structure speeding up data retrieval
what is normalization normalization organizes data to reduce redundancy
what is a join join combines data from multiple tables

# Operating Systems
what is a process process is running instance of program
what is memory memory is storage for data and instructions
what is cpu cpu is processor executing instructions
what is io input output is communication with devices
what is a file system file system organizes data into files directories

# Networks
what is ip address ip address identifies computer on network
what is tcp tcp is protocol ensuring reliable data transmission
what is udp udp is protocol for fast unreliable transmission
what is dns dns translates domain names to ip addresses
what is firewall firewall filters network traffic for security

# Security
what is encryption encryption transforms data into unreadable form
what is authentication authentication verifies identity of user
what is authorization authorization determines access rights
what is a hash hash is one way function producing fixed size output
what is a password password is secret credentials for access

# Software Development
what is version control version control tracks changes to code
what is debugging debugging is finding and fixing code errors
what is testing testing verifies code works as expected
what is documentation documentation describes how code works
what is refactoring refactoring improves code structure without changing behavior

# Machine Learning
what is training training is process of teaching model from data
what is classification classification assigns data to categories
what is regression regression predicts continuous numeric values
what is clustering clustering groups similar data points together
what is overfitting overfitting occurs when model fits training data too closely
"""

def write_corpus(filename, content):
    """Write corpus to file"""
    os.makedirs(os.path.dirname(filename) or '.', exist_ok=True)
    with open(filename, 'w', encoding='utf-8') as f:
        f.write(content)
    
    # Count pairs
    lines = [l.strip() for l in content.split('\n') if l.strip() and not l.startswith('#')]
    print(f"✓ Written {filename}: {len(lines)} Q&A pairs")

def main():
    print("Creating domain-specific Q&A corpora for EH-G3...\n")
    
    write_corpus('training/medical_qa.txt', MEDICAL_QA)
    write_corpus('training/legal_qa.txt', LEGAL_QA)
    write_corpus('training/technical_qa.txt', TECHNICAL_QA)
    
    print("\n=== Summary ===")
    print("Domain corpora created successfully!")
    print("\nUsage for training:")
    print("  python examples/eh_g3_trainer.py training/medical_qa.txt training/medical_embeddings.npy")
    print("  python examples/eh_g3_trainer.py training/legal_qa.txt training/legal_embeddings.npy")
    print("  python examples/eh_g3_trainer.py training/technical_qa.txt training/technical_embeddings.npy")
    print("\nOr combine all for general training:")
    print("  cat training/medical_qa.txt training/legal_qa.txt training/technical_qa.txt > training/combined_qa.txt")
    print("  python examples/eh_g3_trainer.py training/combined_qa.txt training/eh_g3_embeddings.npy")

if __name__ == '__main__':
    main()
